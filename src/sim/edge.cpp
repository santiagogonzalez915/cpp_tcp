#include "nb/edge.hpp"

#include "nb/packet.hpp"
#include "nb/runtime_manager.hpp"

#include <algorithm>
#include <chrono>
#include <random>
#include <stdexcept>
#include <string>

namespace nb {

Edge::Edge(int a, int b, const EdgeOptions& options)
    : a_(a), b_(b), live_(true), options_(options), unit_(0.0, 1.0) {
    // Per-edge RNG, seeded from the global random_device. We only need
    // statistical parity with Java's Math.random() (48-bit LCG).
    std::random_device rd;
    rng_.seed(rd());
}

std::int64_t Edge::schedulePkt(RuntimeManager& manager, int src, int size,
                               std::int64_t nowMicros) {
    if (size > Packet::MAX_PACKET_SIZE) {
        throw std::invalid_argument(
            "Packet size must be less than Packet.MAX_PACKET_SIZE. Size = " +
            std::to_string(size));
    }
    if (src != a_ && src != b_) {
        throw std::invalid_argument(
            "Src specified is not part of this edge");
    }

    std::int64_t& sendTime = (src == a_) ? nextSendTimeA_ : nextSendTimeB_;

    std::int64_t currentPktSendTime = std::max(nowMicros, sendTime);
    // Java: finishTime = cur + size * 1_000_000 / bw
    std::int64_t finishTime =
        currentPktSendTime +
        (static_cast<std::int64_t>(size) * 1000000) / options_.getBW();

    if (finishTime - nowMicros > options_.getBT() * 1000) {
        manager.packetDropped();
        return -1;
    }
    sendTime = finishTime;

    if (!live_ || unit_(rng_) < options_.getLossRate()) {
        manager.packetLost();
        return -1;
    }
    return finishTime + (options_.getDelay() * 1000);
}

} // namespace nb
