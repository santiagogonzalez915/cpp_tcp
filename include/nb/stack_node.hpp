#pragma once

#include "nb/ping_request.hpp"

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace nb {

class RuntimeManager;
class Packet;
class TcpManager;

// StackNode: per-node protocol stack running in the simulator or emulator.
// Mirrors Java Node.java (the student-facing protocol stack node). Handles
// ping/ping-reply and (eventually) dispatches TRANSPORT packets to
// TcpManager.
class StackNode {
public:
    static constexpr std::int64_t PING_TIMEOUT_MS = 10000;

    StackNode(RuntimeManager* manager, int addr);
    ~StackNode();

    void start();

    // Called by the manager when a packet arrives.
    void onReceive(int from, const std::vector<std::uint8_t>& msg);

    // Called by the manager when there is a command for this node from the
    // user (keyboard or command file).
    void onCommand(const std::string& command);

    // Timer callback.
    void pingTimedOut();

    int addr() const { return addr_; }
    RuntimeManager* manager() { return manager_; }
    TcpManager* tcpManager() { return tcpMan_.get(); }

    // Network-layer send entry point for the transport layer, mirroring
    // Java Node.sendSegment.
    void sendSegment(int srcAddr, int destAddr, int protocol,
                     const std::vector<std::uint8_t>& payload);

    void logOutput(const std::string& s) const;
    void logError(const std::string& s) const;

private:
    bool matchPingCommand(const std::string& command);
    bool matchTransferCommand(const std::string& command);
    bool matchServerCommand(const std::string& command);
    void receivePacket(int from, const Packet& packet);
    void receivePing(const Packet& packet);
    void receivePingReply(const Packet& packet);
    void receiveTransport(const Packet& packet);
    void sendPacket(int destAddr, const Packet& packet);

    RuntimeManager* manager_;
    int addr_;
    std::list<PingRequest> pings_;
    std::unique_ptr<TcpManager> tcpMan_;
};

} // namespace nb
