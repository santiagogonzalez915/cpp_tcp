#pragma once

#include <cstdint>
#include <string>

namespace nb {

// EmulatedNode: per-connection state inside the Trawler — the TCP socket
// back to the emulated node, its assigned fish address, and its UDP
// endpoint. Mirrors Java EmulatedNode.java.
class EmulatedNode {
public:
    EmulatedNode(int tcpFd, int fishAddr, const std::string& ipAddress,
                 int udpPort);
    ~EmulatedNode();

    EmulatedNode(const EmulatedNode&) = delete;
    EmulatedNode& operator=(const EmulatedNode&) = delete;

    int fishAddr() const { return fishAddr_; }
    int tcpFd() const { return tcpFd_; }
    const std::string& ipAddress() const { return ipAddress_; }
    int udpPort() const { return udpPort_; }

    bool isAlive() const;
    void close();

    // Write a text line (automatically newline-terminated) to the TCP
    // socket; returns false if the write fails.
    bool writeLine(const std::string& line);

    // Emit add/remove/reset neighbor commands.
    bool sendAddNeighbor(const EmulatedNode& peer);
    bool sendRemoveNeighbor(int peerFishAddr);
    bool sendReset();

private:
    int tcpFd_ = -1;
    int fishAddr_ = -1;
    std::string ipAddress_;
    int udpPort_ = 0;
    bool alive_ = true;
};

} // namespace nb
