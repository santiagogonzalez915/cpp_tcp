#pragma once

#include "nb/runtime_manager.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nb {

class StackNode;
class ConsoleIoThread;
class SimulationCommandsParser;

// Simulator: discrete-event simulator of multiple nodes running the
// protocol stack. Mirrors Java Simulator.java.
class Simulator : public RuntimeManager {
public:
    // MAX_NODES_TO_SIMULATE matches Java (Packet.MAX_ADDRESS - 1 = 254).
    static constexpr int MAX_NODES_TO_SIMULATE = 254;

    Simulator(int numNodes, const std::string& topoFile, long seed = 0);
    ~Simulator() override;

    void start() override;
    bool sendPkt(int from, int to,
                 const std::vector<std::uint8_t>& pkt) override;
    std::int64_t now() const override;
    bool sendNodeMsg(int nodeAddr, const std::string& msg) override;
    void setTimescale(double timescale) override;

    int numNodes() const { return static_cast<int>(nodes_.size()); }

    // Called by the scheduled event queue to re-enter the topology file
    // parser when a `time` deferral expires.
    void parseRestOfTopoFile();

private:
    bool isNodeAddrValid(int nodeAddr) const;
    void deliverPkt(int from, int to, std::vector<std::uint8_t> pkt);

    std::int64_t nowMicros_ = 0;
    double timescale_ = 1.0;
    std::vector<std::unique_ptr<StackNode>> nodes_;
    std::unique_ptr<SimulationCommandsParser> topoFileParser_;
    std::unique_ptr<ConsoleIoThread> ioThread_;
    bool running_ = false;
};

} // namespace nb
