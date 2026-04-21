#pragma once

#include "nb/emulated_link.hpp"
#include "nb/runtime_manager.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace nb {

class StackNode;

// EmulatorARPData: one ARP cache entry (per peer node). Holds the UDP
// endpoint and an optional EmulatedLink for bandwidth/buffer/loss pacing.
struct EmulatorARPData {
    std::string ipAddress;
    int udpPort = 0;
    // When non-null we pace outgoing packets through this link; when null
    // the ARP entry was learned implicitly from a received packet so we
    // defer link emulation until the trawler gives us edge options.
    std::unique_ptr<EmulatedLink> link;
};

// Emulator: real-time emulator running a single local node and talking UDP
// to remote peers coordinated by a Trawler. Mirrors Java Emulator.java.
class Emulator : public RuntimeManager {
public:
    Emulator(const std::string& trawlerHost, int trawlerPort,
             int localUdpPort);
    ~Emulator() override;

    void start() override;
    bool sendPkt(int from, int to,
                 const std::vector<std::uint8_t>& pkt) override;
    std::int64_t now() const override;
    bool sendNodeMsg(int nodeAddr, const std::string& msg) override;

    int assignedNodeId() const { return assignedNodeId_; }

private:
    void connectToTrawler();
    void openUdpSocket();
    void refreshARP();
    void processUdpPacket();
    void handleStdinLine(const std::string& line);
    void schedulePkt(const std::vector<std::uint8_t>& pkt, int destAddr);
    void broadcastPacket(const std::vector<std::uint8_t>& pkt);
    void physicalSend(const std::vector<std::uint8_t>& pkt,
                      const std::string& ipAddress, int udpPort);
    // Exactly one blocking-wait step using poll() with a deadline.
    // `deadlineMicros == -1` means wait indefinitely.
    // Returns true if one or more IO events fired (caller then drains them);
    // false if only the timeout fired.
    bool pollIO(std::int64_t deadlineMicros);

    std::string trawlerHost_;
    int trawlerPort_ = 0;
    int localUdpPort_ = 0;
    int assignedNodeId_ = -1;
    bool running_ = false;

    int trawlerFd_ = -1;
    int udpFd_ = -1;

    // Buffer for partial lines from the trawler TCP socket.
    std::string trawlerBuf_;

    // poll events latched inside pollIO for the next drain step.
    bool udpReady_ = false;
    bool trawlerReady_ = false;
    bool stdinReady_ = false;
    bool stdinEof_ = false;

    std::unordered_map<int, std::unique_ptr<EmulatorARPData>> arp_;
    std::unique_ptr<StackNode> node_;
};

} // namespace nb
