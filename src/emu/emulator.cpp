#include "nb/emulator.hpp"

#include "nb/clock.hpp"
#include "nb/emulated_link.hpp"
#include "nb/emulation_commands_parser.hpp"
#include "nb/emulator_packet.hpp"
#include "nb/packet.hpp"
#include "nb/stack_node.hpp"
#include "nb/trawler_node_arp_commands.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace nb {

namespace {

bool writeAllFd(int fd, const std::string& s) {
    std::size_t off = 0;
    while (off < s.size()) {
        ssize_t n = ::send(fd, s.data() + off, s.size() - off, 0);
        if (n <= 0) return false;
        off += static_cast<std::size_t>(n);
    }
    return true;
}

// Resolve an IPv4 host to the first A record.
bool resolveHost(const std::string& host, in_addr& outAddr) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (rc != 0 || !res) return false;
    auto* sa = reinterpret_cast<sockaddr_in*>(res->ai_addr);
    outAddr = sa->sin_addr;
    ::freeaddrinfo(res);
    return true;
}

} // namespace

Emulator::Emulator(const std::string& trawlerHost, int trawlerPort,
                   int localUdpPort)
    : RuntimeManager(clockMicros()),
      trawlerHost_(trawlerHost),
      trawlerPort_(trawlerPort),
      localUdpPort_(localUdpPort) {
    setParser(std::make_unique<EmulationCommandsParser>(this));
}

Emulator::~Emulator() {
    if (trawlerFd_ >= 0) ::close(trawlerFd_);
    if (udpFd_ >= 0) ::close(udpFd_);
}

void Emulator::connectToTrawler() {
    in_addr host{};
    if (!resolveHost(trawlerHost_, host)) {
        throw std::runtime_error("Could not resolve trawler host: " +
                                 trawlerHost_);
    }
    trawlerFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (trawlerFd_ < 0) {
        throw std::runtime_error("trawler socket() failed");
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr = host;
    addr.sin_port = htons(trawlerPort_);
    if (::connect(trawlerFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))
        < 0) {
        throw std::runtime_error("connect() to trawler failed: " +
                                 std::string(std::strerror(errno)));
    }
    int nodelay = 1;
    ::setsockopt(trawlerFd_, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                 sizeof(nodelay));
}

void Emulator::openUdpSocket() {
    udpFd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (udpFd_ < 0) throw std::runtime_error("udp socket() failed");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(localUdpPort_);
    if (::bind(udpFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::invalid_argument("Illegal local udp port " +
                                    std::to_string(localUdpPort_));
    }
}

void Emulator::start() {
    connectToTrawler();
    openUdpSocket();

    // Handshake: send local UDP port as a line; receive fish address back.
    if (!writeAllFd(trawlerFd_, std::to_string(localUdpPort_) + "\n")) {
        throw std::runtime_error("Failed to send udp port to trawler");
    }
    std::string line;
    char c;
    while (true) {
        ssize_t n = ::recv(trawlerFd_, &c, 1, 0);
        if (n <= 0) {
            throw std::runtime_error("Trawler closed during handshake");
        }
        if (c == '\n') break;
        if (c == '\r') continue;
        line += c;
    }
    try {
        assignedNodeId_ = std::stoi(line);
    } catch (...) {
        std::cerr << "Msg received from trawler is not an int, thus is not a "
                     "fish address!!" << std::endl;
        std::exit(1);
    }
    if (assignedNodeId_ == Packet::BROADCAST_ADDRESS) {
        std::cerr << "Port " << localUdpPort_
                  << " is already in use. Pick another" << std::endl;
        throw std::invalid_argument("Illegal local port " +
                                    std::to_string(localUdpPort_));
    }

    node_ = std::make_unique<StackNode>(this, assignedNodeId_);
    node_->start();

    // Make stdin non-blocking so we can read whatever's ready without
    // getting stuck when the user hasn't typed a full line.
    int stdinFlags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
    if (stdinFlags >= 0) {
        ::fcntl(STDIN_FILENO, F_SETFL, stdinFlags | O_NONBLOCK);
    }
    static std::string stdinBuf;

    std::int64_t deferParsingTill = 0;
    running_ = true;

    while (running_) {
        std::int64_t nowMicros = clockMicros();
        deferParsingTill = readCommandFile(deferParsingTill);

        // Run all due events.
        while (sortedEvents_.getNextEvent() &&
               sortedEvents_.getNextEvent()->timeToOccur <= nowMicros) {
            ScheduledEvent ev = sortedEvents_.removeNextEvent();
            try {
                ev.callback();
            } catch (const std::exception& e) {
                std::cerr << "Exception while invoking method in Emulator: "
                          << e.what() << std::endl;
            }
        }

        std::int64_t waitTill;
        const ScheduledEvent* next = sortedEvents_.getNextEvent();
        if (!next) {
            waitTill = deferParsingTill;
        } else if (deferParsingTill == -1) {
            waitTill = next->timeToOccur;
        } else if (deferParsingTill == 0) {
            // defer 0 means parse immediately next iteration — no blocking
            waitTill = clockMicros();
        } else {
            waitTill = std::min<std::int64_t>(next->timeToOccur,
                                              deferParsingTill);
        }

        if (waitTill == -1 || clockMicros() < waitTill) {
            pollIO(waitTill);
        }

        // Always refresh ARP (trawler updates may have arrived) — drain
        // every pending line in the trawler buffer before doing anything
        // else.
        refreshARP();

        if (udpReady_) {
            udpReady_ = false;
            processUdpPacket();
        }
        if (stdinReady_) {
            stdinReady_ = false;
            char buf[1024];
            while (true) {
                ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
                if (n == 0) {
                    stdinEof_ = true;
                    break;
                }
                if (n < 0) break;
                stdinBuf.append(buf, buf + n);
            }
            std::size_t pos;
            while ((pos = stdinBuf.find('\n')) != std::string::npos) {
                std::string line = stdinBuf.substr(0, pos);
                stdinBuf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                handleStdinLine(line);
            }
        }
    }
}

bool Emulator::pollIO(std::int64_t deadlineMicros) {
    pollfd pfds[3];
    int n = 0;
    pfds[n].fd = udpFd_;
    pfds[n].events = POLLIN;
    pfds[n].revents = 0;
    int udpIdx = n++;
    pfds[n].fd = trawlerFd_;
    pfds[n].events = POLLIN;
    pfds[n].revents = 0;
    int trawlerIdx = n++;
    int stdinIdx = -1;
    if (!stdinEof_) {
        pfds[n].fd = STDIN_FILENO;
        pfds[n].events = POLLIN;
        pfds[n].revents = 0;
        stdinIdx = n++;
    }

    int timeoutMs = -1;
    if (deadlineMicros != -1) {
        std::int64_t delta = deadlineMicros - clockMicros();
        if (delta < 0) delta = 0;
        timeoutMs = static_cast<int>((delta + 999) / 1000);
    }

    int rc = ::poll(pfds, n, timeoutMs);
    if (rc < 0) {
        if (errno == EINTR) return false;
        std::cerr << "poll() failed: " << std::strerror(errno) << std::endl;
        return false;
    }
    if (rc == 0) return false;

    udpReady_ = (pfds[udpIdx].revents & POLLIN) != 0;
    trawlerReady_ = (pfds[trawlerIdx].revents & POLLIN) != 0;
    if (stdinIdx >= 0) {
        short rev = pfds[stdinIdx].revents;
        if (rev & POLLIN) {
            stdinReady_ = true;
        }
        // POLLHUP / POLLNVAL: stdin has been closed (e.g. process was
        // launched with no terminal); don't keep spinning on it.
        if (rev & (POLLHUP | POLLNVAL | POLLERR)) {
            stdinEof_ = true;
        }
    }
    return true;
}

void Emulator::refreshARP() {
    // Drain every complete line from the trawler socket without blocking.
    while (true) {
        char buf[1024];
        ssize_t n = ::recv(trawlerFd_, buf, sizeof(buf), MSG_DONTWAIT);
        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            if (n == 0) {
                std::cerr << "Trawler socket closed" << std::endl;
                std::exit(1);
            }
            break;
        }
        trawlerBuf_.append(buf, buf + n);
    }
    std::size_t pos;
    while ((pos = trawlerBuf_.find('\n')) != std::string::npos) {
        std::string line = trawlerBuf_.substr(0, pos);
        trawlerBuf_.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (TrawlerNodeARPCommands::receiveReset(line)) {
            arp_.clear();
            continue;
        }
        int rm = TrawlerNodeARPCommands::receiveRemoveNeighbor(line);
        if (rm >= 0) {
            arp_.erase(rm);
            continue;
        }
        auto add = TrawlerNodeARPCommands::receiveAddNeighbor(line);
        if (add && Packet::validAddress(add->nodeId)) {
            auto entry = std::make_unique<EmulatorARPData>();
            entry->ipAddress = add->ipAddress;
            entry->udpPort = add->port;
            if (add->options) {
                entry->link =
                    std::make_unique<EmulatedLink>(*add->options);
            }
            arp_[add->nodeId] = std::move(entry);
            continue;
        }
        std::cerr << "Unrecognized command from trawler: " << line
                  << std::endl;
    }
}

void Emulator::processUdpPacket() {
    std::uint8_t buf[65535];
    sockaddr_in src{};
    socklen_t srcLen = sizeof(src);
    ssize_t n = ::recvfrom(udpFd_, buf, sizeof(buf), 0,
                           reinterpret_cast<sockaddr*>(&src), &srcLen);
    if (n <= 0) return;
    auto packetOpt =
        EmulatorPacket::unpack(std::vector<std::uint8_t>(buf, buf + n));
    if (!packetOpt) {
        char ip[INET_ADDRSTRLEN] = {0};
        ::inet_ntop(AF_INET, &src.sin_addr, ip, sizeof(ip));
        std::cerr << "Was unable to extract packet received from " << ip
                  << ":" << ntohs(src.sin_port) << std::endl;
        return;
    }
    int srcAddr = packetOpt->getSrc();
    int destAddr = packetOpt->getDest();
    char ipBuf[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &src.sin_addr, ipBuf, sizeof(ipBuf));
    std::string ip(ipBuf);
    int port = ntohs(src.sin_port);

    // Learn / refresh ARP entry for the sender (deferred emulation until
    // trawler supplies edge options).
    auto it = arp_.find(srcAddr);
    bool newArp = false;
    if (it == arp_.end()) {
        newArp = true;
    } else if (it->second->ipAddress != ip || it->second->udpPort != port) {
        newArp = true;
    }
    if (newArp) {
        auto entry = std::make_unique<EmulatorARPData>();
        entry->ipAddress = ip;
        entry->udpPort = port;
        arp_[srcAddr] = std::move(entry);
    }

    if (destAddr == assignedNodeId_ || destAddr == Packet::BROADCAST_ADDRESS) {
        node_->onReceive(srcAddr, packetOpt->getPayload());
    }
}

void Emulator::handleStdinLine(const std::string& line) {
    if (line.empty()) return;
    parser()->parseLine(line, clockMicros());
}

bool Emulator::sendPkt(int from, int to,
                       const std::vector<std::uint8_t>& pkt) {
    RuntimeManager::sendPkt(from, to, pkt);
    // Pull in any pending ARP updates before we rely on the cache.
    refreshARP();

    EmulatorPacket epkt(to, from, pkt);
    std::vector<std::uint8_t> payload = epkt.pack();
    if (payload.empty()) return false;

    try {
        if (to == Packet::BROADCAST_ADDRESS) {
            for (const auto& kv : arp_) {
                schedulePkt(payload, kv.first);
            }
        } else if (arp_.find(to) != arp_.end()) {
            schedulePkt(payload, to);
        } else {
            std::cerr << "Node " << to << " is not a neighbor of node " << from
                      << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception while trying to send to node: " << to
                  << ". Exception: " << e.what() << std::endl;
        return false;
    }
    return true;
}

void Emulator::schedulePkt(const std::vector<std::uint8_t>& pkt, int destAddr) {
    auto it = arp_.find(destAddr);
    if (it == arp_.end()) return;
    EmulatorARPData& data = *it->second;
    if (!data.link) {
        physicalSend(pkt, data.ipAddress, data.udpPort);
        return;
    }
    // Payload size used for bandwidth/buffer accounting is the inner
    // (Packet) byte count, matching Java where `pkt.length` is the inner
    // byte array passed into Emulator.sendPkt. We approximate by using
    // the UDP payload minus the 3-byte EmulatorPacket header.
    int size = static_cast<int>(pkt.size()) - EmulatorPacket::HEADER_SIZE;
    if (size < 0) size = static_cast<int>(pkt.size());
    std::int64_t currentTime = clockMicros();
    std::int64_t timeToDeliver =
        data.link->schedulePkt(*this, size, currentTime);
    if (timeToDeliver == -1) return;
    std::int64_t usecFraction = timeToDeliver % 1000;
    if (usecFraction != 0) timeToDeliver += 1000 - usecFraction;

    std::string ip = data.ipAddress;
    int port = data.udpPort;
    addEventAtMicros(timeToDeliver, [this, pkt, ip, port]() {
        physicalSend(pkt, ip, port);
    });
}

void Emulator::physicalSend(const std::vector<std::uint8_t>& pkt,
                            const std::string& ipAddress, int udpPort) {
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(udpPort);
    if (::inet_pton(AF_INET, ipAddress.c_str(), &dst.sin_addr) <= 0) {
        std::cerr << "inet_pton failed for " << ipAddress << std::endl;
        return;
    }
    ssize_t n = ::sendto(udpFd_, pkt.data(), pkt.size(), 0,
                         reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    if (n < 0) {
        std::cerr << "sendto() failed: " << std::strerror(errno) << std::endl;
    }
}

std::int64_t Emulator::now() const { return clockMicros() / 1000; }

bool Emulator::sendNodeMsg(int /*nodeAddr*/, const std::string& msg) {
    if (node_) node_->onCommand(msg);
    return true;
}

} // namespace nb
