#include "nb/stack_node.hpp"

#include "nb/packet.hpp"
#include "nb/protocol.hpp"
#include "nb/runtime_manager.hpp"
#include "nb/string_bytes.hpp"
#include "nb/tcp_manager.hpp"
#include "nb/tcp_socket.hpp"
#include "nb/transfer_client.hpp"
#include "nb/transfer_server.hpp"
#include "nb/transport.hpp"

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace nb {

StackNode::StackNode(RuntimeManager* manager, int addr)
    : manager_(manager), addr_(addr) {
    tcpMan_ = std::make_unique<TcpManager>(this, addr, manager);
}

StackNode::~StackNode() = default;

void StackNode::start() {
    logOutput("started");
    manager_->addTimer(addr_, PING_TIMEOUT_MS,
                       [this] { pingTimedOut(); });
    tcpMan_->start();
}

void StackNode::onReceive(int from, const std::vector<std::uint8_t>& msg) {
    auto pkt = Packet::unpack(msg);
    if (!pkt.has_value()) {
        logError("Unable to unpack message received from " +
                 std::to_string(from));
        return;
    }
    receivePacket(from, *pkt);
}

void StackNode::onCommand(const std::string& command) {
    if (matchTransferCommand(command)) return;
    if (matchServerCommand(command)) return;
    if (matchPingCommand(command)) return;
    logError("Unrecognized command: " + command);
}

namespace {
std::vector<std::string> splitSpaceVec(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ' ') { out.push_back(std::move(cur)); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(std::move(cur));
    return out;
}
} // namespace

bool StackNode::matchTransferCommand(const std::string& command) {
    // transfer dest port localPort amount [interval sz]
    auto args = splitSpaceVec(command);
    if (args.size() < 5 || args.size() > 7 || args[0] != "transfer")
        return false;
    try {
        int destAddr = std::stoi(args[1]);
        int port = std::stoi(args[2]);
        int localPort = std::stoi(args[3]);
        int amount = std::stoi(args[4]);
        long interval = args.size() >= 6
                            ? std::stol(args[5])
                            : TransferClient::DEFAULT_CLIENT_INTERVAL;
        int sz = args.size() == 7 ? std::stoi(args[6])
                                   : TransferClient::DEFAULT_BUFFER_SZ;
        TcpSocket* sock = tcpMan_->socket();
        sock->bind(localPort);
        sock->connect(destAddr, port);
        auto client = std::make_shared<TransferClient>(
            manager_, this, sock, amount, interval, sz);
        client->start();
        return true;
    } catch (const std::exception& e) {
        logError(std::string("Exception: ") + e.what());
    }
    return false;
}

bool StackNode::matchServerCommand(const std::string& command) {
    // server port backlog [servint workint sz]
    auto args = splitSpaceVec(command);
    if (args.size() < 3 || args.size() > 6 || args[0] != "server")
        return false;
    try {
        int port = std::stoi(args[1]);
        int backlog = std::stoi(args[2]);
        long servint = args.size() >= 4
                           ? std::stol(args[3])
                           : TransferServer::DEFAULT_SERVER_INTERVAL;
        long workint = args.size() >= 5
                           ? std::stol(args[4])
                           : TransferServer::DEFAULT_WORKER_INTERVAL;
        int sz = args.size() == 6 ? std::stoi(args[5])
                                   : TransferServer::DEFAULT_BUFFER_SZ;
        TcpSocket* sock = tcpMan_->socket();
        sock->bind(port);
        sock->listen(backlog);
        auto server = std::make_shared<TransferServer>(
            manager_, this, sock, servint, workint, sz);
        server->start();
        logOutput("server started, port = " + std::to_string(port));
        return true;
    } catch (const std::exception& e) {
        logError(std::string("Exception: ") + e.what());
    }
    return false;
}

void StackNode::sendSegment(int srcAddr, int destAddr, int protocol,
                             const std::vector<std::uint8_t>& payload) {
    Packet pkt(destAddr, srcAddr, Packet::MAX_TTL, protocol, 0, payload);
    sendPacket(destAddr, pkt);
}

void StackNode::pingTimedOut() {
    auto now = manager_->now();
    for (auto it = pings_.begin(); it != pings_.end();) {
        if (it->timeSentMs + PING_TIMEOUT_MS < now) {
            std::ostringstream oss;
            oss << "Timing out ping: Dest: " << it->destAddr
                << " Send Time: " << it->timeSentMs
                << " Message: " << bytesToString(it->msg);
            logOutput(oss.str());
            it = pings_.erase(it);
        } else {
            ++it;
        }
    }
    manager_->addTimer(addr_, PING_TIMEOUT_MS,
                       [this] { pingTimedOut(); });
}

bool StackNode::matchPingCommand(const std::string& command) {
    auto space = command.find(' ');
    if (space == std::string::npos) return false;
    try {
        int dest = std::stoi(command.substr(0, space));
        std::string msg = command.substr(space + 1);
        auto bytes = stringToBytes(msg);
        Packet pkt(dest, addr_, Packet::MAX_TTL, Protocol::PING_PKT, 0,
                   bytes);
        sendPacket(dest, pkt);
        pings_.emplace_back(dest, bytes, manager_->now());
        return true;
    } catch (const std::exception& e) {
        logError(std::string("Exception: ") + e.what());
    }
    return false;
}

void StackNode::receivePacket(int /*from*/, const Packet& packet) {
    switch (packet.getProtocol()) {
    case Protocol::PING_PKT:
        receivePing(packet);
        break;
    case Protocol::PING_REPLY_PKT:
        receivePingReply(packet);
        break;
    case Protocol::TRANSPORT_PKT:
        receiveTransport(packet);
        break;
    default:
        logError("Packet with unknown protocol received. Protocol: " +
                 std::to_string(packet.getProtocol()));
    }
}

void StackNode::receivePing(const Packet& packet) {
    logOutput("Received Ping from " + std::to_string(packet.getSrc()) +
              " with message: " + bytesToString(packet.getPayload()));
    try {
        Packet reply(packet.getSrc(), addr_, Packet::MAX_TTL,
                     Protocol::PING_REPLY_PKT, 0, packet.getPayload());
        sendPacket(packet.getSrc(), reply);
    } catch (const std::exception& e) {
        logError(std::string("Exception while sending Ping Reply: ") +
                 e.what());
    }
}

void StackNode::receivePingReply(const Packet& packet) {
    std::string payload = bytesToString(packet.getPayload());
    for (auto it = pings_.begin(); it != pings_.end(); ++it) {
        if (it->destAddr == packet.getSrc() &&
            bytesToString(it->msg) == payload) {
            logOutput("Got Ping Reply from " +
                      std::to_string(packet.getSrc()) + ": " + payload);
            pings_.erase(it);
            return;
        }
    }
    logError("Unexpected Ping Reply from " +
             std::to_string(packet.getSrc()) + ": " + payload);
}

void StackNode::receiveTransport(const Packet& packet) {
    auto t = Transport::unpack(packet.getPayload());
    if (!t.has_value()) {
        logError("Unable to unpack Transport packet from " +
                 std::to_string(packet.getSrc()));
        return;
    }
    tcpMan_->receiveTransport(packet.getSrc(), *t);
}

void StackNode::sendPacket(int destAddr, const Packet& packet) {
    try {
        manager_->sendPkt(addr_, destAddr, packet.pack());
    } catch (const std::exception& e) {
        logError(std::string("Exception: ") + e.what());
    }
}

void StackNode::logOutput(const std::string& s) const {
    std::cout << "Node " << addr_ << ": " << s << std::endl;
}

void StackNode::logError(const std::string& s) const {
    std::cerr << "Node " << addr_ << ": " << s << std::endl;
}

} // namespace nb
