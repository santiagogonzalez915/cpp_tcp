#include "nb/simulator.hpp"

#include "nb/clock.hpp"
#include "nb/console_io_thread.hpp"
#include "nb/edge.hpp"
#include "nb/packet.hpp"
#include "nb/simulation_commands_parser.hpp"
#include "nb/stack_node.hpp"
#include "nb/topology.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace nb {

Simulator::Simulator(int numNodes, const std::string& topoFile, long /*seed*/)
    : RuntimeManager(0) {
    if (numNodes <= 0 || numNodes > MAX_NODES_TO_SIMULATE) {
        throw std::invalid_argument(
            "Simulator: invalid number of nodes: " + std::to_string(numNodes));
    }
    Topology::reset();
    (void)Topology::instance(false);

    setParser(std::make_unique<SimulationCommandsParser>(this));
    topoFileParser_ = std::make_unique<SimulationCommandsParser>(this);

    nodes_.reserve(numNodes);
    for (int i = 0; i < numNodes; ++i) {
        nodes_.push_back(std::make_unique<StackNode>(this, i));
    }

    // Parse the topology file (which may contain `time` directives that
    // defer further parsing to the main loop).
    std::int64_t deferTill = 0;
    if (!topoFile.empty()) {
        deferTill = topoFileParser_->parseFile(topoFile, nowMicros_);
    } else {
        deferTill = -1;
    }
    if (deferTill >= 0) {
        addEventAtMicros(deferTill, [this] { parseRestOfTopoFile(); });
    }

    ioThread_ = std::make_unique<ConsoleIoThread>();
    ioThread_->start();
}

Simulator::~Simulator() = default;

void Simulator::start() {
    running_ = true;
    for (auto& n : nodes_) {
        n->start();
    }
    nowMicros_ = 1;

    std::int64_t deferParsingTill = 0;
    while (running_) {
        std::int64_t deltaTime = 0;
        deferParsingTill = readCommandFile(deferParsingTill);

        // Pick the next wake-up source.
        std::int64_t waitTime = -1;  // indefinite
        const ScheduledEvent* nextEvent = sortedEvents_.getNextEvent();
        if (nextEvent) {
            deltaTime = nextEvent->timeToOccur - nowMicros_;
            waitTime = 0;
        } else if (deferParsingTill >= 0) {
            deltaTime = deferParsingTill - nowMicros_;
            waitTime = 0;
        }

        if (deltaTime > 0 && timescale_ > 0) {
            waitTime = static_cast<long>(
                static_cast<double>(deltaTime) / timescale_);
        }

        // Convert microseconds → milliseconds for waitForLine.
        long waitMs = waitTime < 0 ? -1 : waitTime / 1000;
        std::int64_t beforeInput = clockMicros();
        auto userInput = ioThread_->waitForLine(waitMs);

        if (userInput) {
            nowMicros_ += static_cast<std::int64_t>(
                (clockMicros() - beforeInput) * timescale_);
            if (parser()) {
                parser()->parseLine(*userInput, nowMicros_);
            }
        } else {
            nowMicros_ = std::max(nowMicros_, nowMicros_ + deltaTime);
            while (true) {
                const ScheduledEvent* peek = sortedEvents_.getNextEvent();
                if (!peek || peek->timeToOccur > nowMicros_) break;
                auto ev = sortedEvents_.removeNextEvent();
                try {
                    if (ev.callback) ev.callback();
                } catch (const std::exception& e) {
                    std::cerr << "Simulator: callback threw: " << e.what()
                              << std::endl;
                }
            }
        }
    }
}

bool Simulator::sendPkt(int from, int to,
                        const std::vector<std::uint8_t>& pkt) {
    RuntimeManager::sendPkt(from, to, pkt);

    auto& topo = Topology::instance();
    if (to == Packet::BROADCAST_ADDRESS) {
        for (int i = 0; i < static_cast<int>(nodes_.size()); ++i) {
            Edge* edge = topo.getLiveEdge(from, i);
            if (edge) {
                std::int64_t at = edge->schedulePkt(
                    *this, from, static_cast<int>(pkt.size()), nowMicros_);
                if (at != -1) {
                    auto copy = pkt;
                    addEventAtMicros(
                        at, [this, from, i, copy = std::move(copy)]() mutable {
                            deliverPkt(from, i, std::move(copy));
                        });
                }
            }
        }
        return true;
    }
    Edge* edge = topo.getLiveEdge(from, to);
    if (!edge) {
        std::cerr << "Failed to send pkt from: " << from << " to: " << to
                  << std::endl;
        return false;
    }
    std::int64_t at =
        edge->schedulePkt(*this, from, static_cast<int>(pkt.size()),
                          nowMicros_);
    if (at == -1) return true;  // dropped or lost, already counted
    auto copy = pkt;
    addEventAtMicros(at, [this, from, to, copy = std::move(copy)]() mutable {
        deliverPkt(from, to, std::move(copy));
    });
    return true;
}

std::int64_t Simulator::now() const { return nowMicros_ / 1000; }

bool Simulator::sendNodeMsg(int nodeAddr, const std::string& msg) {
    if (!isNodeAddrValid(nodeAddr)) return false;
    nodes_[nodeAddr]->onCommand(msg);
    return true;
}

void Simulator::setTimescale(double timescale) { timescale_ = timescale; }

void Simulator::parseRestOfTopoFile() {
    std::int64_t deferTill =
        topoFileParser_ ? topoFileParser_->parseRemainder(nowMicros_) : -1;
    if (deferTill >= 0) {
        addEventAtMicros(deferTill, [this] { parseRestOfTopoFile(); });
    }
}

bool Simulator::isNodeAddrValid(int nodeAddr) const {
    return nodeAddr >= 0 &&
           nodeAddr < static_cast<int>(nodes_.size());
}

void Simulator::deliverPkt(int from, int to,
                            std::vector<std::uint8_t> pkt) {
    if (isNodeAddrValid(to)) {
        nodes_[to]->onReceive(from, pkt);
    }
}

} // namespace nb
