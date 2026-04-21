#pragma once

#include "nb/edge_options.hpp"

#include <cstdint>
#include <random>

namespace nb {

class RuntimeManager;

// Edge: bandwidth / delay / loss pacer for a pair of nodes.
// Mirrors Java Edge.schedulePkt(Manager, src, size, now).
class Edge {
public:
    Edge(int a, int b, const EdgeOptions& options);

    // Return the microsecond arrival time of the packet at the destination,
    // or -1 if the packet was dropped/lost. manager is used to report
    // dropped/lost packet stats.
    std::int64_t schedulePkt(RuntimeManager& manager, int src, int size,
                             std::int64_t nowMicros);

    bool isEdge(int a, int b) const {
        return (a_ == a && b_ == b) || (a_ == b && b_ == a);
    }

    int getNodeA() const { return a_; }
    int getNodeB() const { return b_; }

    bool isLive() const { return live_; }
    void setState(bool live) { live_ = live; }

    const EdgeOptions& getOptions() const { return options_; }
    void setOptions(const EdgeOptions& options) { options_ = options; }

private:
    int a_;
    int b_;
    bool live_;
    std::int64_t nextSendTimeA_ = 0;
    std::int64_t nextSendTimeB_ = 0;
    EdgeOptions options_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> unit_;
};

} // namespace nb
