#include "nb/trawler_service.hpp"

#include "nb/clock.hpp"
#include "nb/emulated_node.hpp"
#include "nb/packet.hpp"
#include "nb/topology.hpp"
#include "nb/trawler_commands_parser.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace nb {

namespace {

// Read a single newline-terminated line from a TCP socket. Returns empty
// string and sets `closed` if the peer closed or an error occurred.
std::string readLineFromFd(int fd, bool& closed) {
    std::string line;
    char c;
    closed = false;
    while (true) {
        ssize_t n = ::recv(fd, &c, 1, 0);
        if (n == 0) {
            closed = true;
            return line;
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            closed = true;
            return line;
        }
        if (c == '\n') break;
        if (c == '\r') continue;
        line += c;
    }
    return line;
}

bool writeLineToFd(int fd, const std::string& line) {
    std::string withNl = line + '\n';
    std::size_t off = 0;
    while (off < withNl.size()) {
        ssize_t n = ::send(fd, withNl.data() + off, withNl.size() - off, 0);
        if (n <= 0) return false;
        off += static_cast<std::size_t>(n);
    }
    return true;
}

std::string addrToString(const sockaddr_in& addr) {
    char buf[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}

} // namespace

TrawlerService::TrawlerService(int listenPort, const std::string& topoFile,
                               bool allToAll)
    : listenPort_(listenPort), topoFile_(topoFile), allToAll_(allToAll) {
    parser_ = std::make_unique<TrawlerCommandsParser>(this);
}

TrawlerService::~TrawlerService() {
    if (listenFd_ >= 0) ::close(listenFd_);
}

void TrawlerService::run() {
    Topology::instance(allToAll_);

    std::int64_t deferParsingTill = -1;
    if (!topoFile_.empty()) {
        deferParsingTill = parser_->parseFile(topoFile_, clockMicros());
    }

    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        throw std::runtime_error("socket() failed");
    }
    int yes = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(listenPort_);
    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))
        < 0) {
        throw std::runtime_error("bind() failed on port " +
                                 std::to_string(listenPort_));
    }
    if (::listen(listenFd_, 16) < 0) {
        throw std::runtime_error("listen() failed");
    }

    running_ = true;
    std::cout << "Trawler awaiting fish..." << std::endl;

    while (running_) {
        sockaddr_in peer{};
        socklen_t peerLen = sizeof(peer);
        int clientFd = ::accept(listenFd_,
                                reinterpret_cast<sockaddr*>(&peer), &peerLen);
        if (clientFd < 0) {
            if (errno == EINTR) continue;
            std::cerr << "Trawler: accept() failed: " << std::strerror(errno)
                      << std::endl;
            break;
        }

        checkNodesQuit();

        std::int64_t now = clockMicros();
        if (deferParsingTill > -1 && deferParsingTill < now) {
            deferParsingTill = parser_->parseRemainder(now);
        }

        bool closed = false;
        std::string portLine = readLineFromFd(clientFd, closed);
        if (closed) {
            ::close(clientFd);
            continue;
        }
        int port = 0;
        try {
            port = std::stoi(portLine);
        } catch (...) {
            std::cerr << "Msg received from node is not a port number.\n";
            ::close(clientFd);
            continue;
        }
        std::string ipAddress = addrToString(peer);

        if (port < 1024 || portConflict(ipAddress, port)) {
            std::cerr << "Trawler: Illegal port: " << port << std::endl;
            writeLineToFd(clientFd, std::to_string(Packet::BROADCAST_ADDRESS));
            ::close(clientFd);
            continue;
        }

        int fishAddr = freeFishAddr();
        if (fishAddr == -1) {
            std::cerr << "Trawler: out of addresses" << std::endl;
            writeLineToFd(clientFd, std::to_string(Packet::BROADCAST_ADDRESS));
            ::close(clientFd);
            continue;
        }

        int nodelay = 1;
        ::setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                     sizeof(nodelay));

        std::cout << "Got port " << port << ": assigning addr: " << fishAddr
                  << std::endl;
        writeLineToFd(clientFd, std::to_string(fishAddr));

        auto node = std::make_unique<EmulatedNode>(clientFd, fishAddr,
                                                   ipAddress, port);
        emulatedNodes_[fishAddr] = std::move(node);
        updateNeighbors(fishAddr);
    }
}

void TrawlerService::startEdge(int a, int b) {
    EmulatedNode* nA = getEmulatedNode(a);
    EmulatedNode* nB = getEmulatedNode(b);
    if (nA && nB) {
        nA->sendAddNeighbor(*nB);
        nB->sendAddNeighbor(*nA);
    }
}

void TrawlerService::failEdge(int a, int b) {
    EmulatedNode* nA = getEmulatedNode(a);
    EmulatedNode* nB = getEmulatedNode(b);
    if (nA && nB) {
        nA->sendRemoveNeighbor(b);
        nB->sendRemoveNeighbor(a);
    }
}

void TrawlerService::failNode(int a) {
    EmulatedNode* node = getEmulatedNode(a);
    if (node) {
        node->sendReset();
        removeAsNeighbor(node);
    }
}

void TrawlerService::restartNode(int a) { updateNeighbors(a); }

void TrawlerService::exit() {
    std::cout << "Trawler exiting..." << std::endl;
    std::exit(0);
}

void TrawlerService::remove(EmulatedNode* dyingNode) {
    std::cerr << "Removing node " << dyingNode->fishAddr() << std::endl;
    int dyingAddr = dyingNode->fishAddr();
    for (auto it = emulatedNodes_.begin(); it != emulatedNodes_.end();) {
        if (it->second->fishAddr() == dyingAddr) {
            it = emulatedNodes_.erase(it);
        } else {
            if (Topology::instance().getLiveEdge(it->second->fishAddr(),
                                                 dyingAddr) != nullptr &&
                it->second->isAlive()) {
                it->second->sendRemoveNeighbor(dyingAddr);
            }
            ++it;
        }
    }
}

void TrawlerService::checkNodesQuit() {
    for (auto it = emulatedNodes_.begin(); it != emulatedNodes_.end();) {
        if (!it->second->isAlive()) {
            it->second->close();
            int addr = it->second->fishAddr();
            EmulatedNode* ptr = it->second.get();
            (void)ptr;
            it = emulatedNodes_.erase(it);
            for (auto& kv : emulatedNodes_) {
                if (Topology::instance().getLiveEdge(kv.second->fishAddr(),
                                                     addr) != nullptr &&
                    kv.second->isAlive()) {
                    kv.second->sendRemoveNeighbor(addr);
                }
            }
        } else {
            ++it;
        }
    }
}

void TrawlerService::updateNeighbors(int fishAddr) {
    EmulatedNode* startingNode = getEmulatedNode(fishAddr);
    if (!startingNode) return;
    for (auto& kv : emulatedNodes_) {
        EmulatedNode* node = kv.second.get();
        if (node->fishAddr() != fishAddr &&
            Topology::instance().getLiveEdge(node->fishAddr(),
                                             startingNode->fishAddr())
                != nullptr &&
            node->isAlive()) {
            node->sendAddNeighbor(*startingNode);
            startingNode->sendAddNeighbor(*node);
        }
    }
}

void TrawlerService::removeAsNeighbor(EmulatedNode* dyingNode) {
    int dyingAddr = dyingNode->fishAddr();
    for (auto& kv : emulatedNodes_) {
        if (kv.second.get() == dyingNode) continue;
        if (Topology::instance().getLiveEdge(kv.second->fishAddr(), dyingAddr)
            != nullptr &&
            kv.second->isAlive()) {
            kv.second->sendRemoveNeighbor(dyingAddr);
        }
    }
}

int TrawlerService::freeFishAddr() {
    for (int i = 0; i < Packet::BROADCAST_ADDRESS; ++i) {
        if (emulatedNodes_.find(i) == emulatedNodes_.end()) return i;
    }
    return -1;
}

bool TrawlerService::portConflict(const std::string& ipAddress, int udpPort) {
    for (const auto& kv : emulatedNodes_) {
        if (kv.second->ipAddress() == ipAddress &&
            kv.second->udpPort() == udpPort) {
            return true;
        }
    }
    return false;
}

EmulatedNode* TrawlerService::getEmulatedNode(int fishAddr) {
    auto it = emulatedNodes_.find(fishAddr);
    return it == emulatedNodes_.end() ? nullptr : it->second.get();
}

} // namespace nb
