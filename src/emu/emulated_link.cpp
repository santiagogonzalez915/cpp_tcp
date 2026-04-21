#include "nb/emulated_link.hpp"

#include "nb/runtime_manager.hpp"

#include <algorithm>
#include <random>

namespace nb {

EmulatedLink::EmulatedLink(const EdgeOptions& options)
    : options_(options), unit_(0.0, 1.0) {
    std::random_device rd;
    rng_.seed(rd());
}

std::int64_t EmulatedLink::schedulePkt(RuntimeManager& manager, int size,
                                       std::int64_t nowMicros) {
    std::int64_t currentPktSendTime = std::max(nowMicros, nextSendTime_);
    std::int64_t finishTime =
        currentPktSendTime +
        (static_cast<std::int64_t>(size) * 1000000) / options_.getBW();

    if (finishTime - nowMicros > options_.getBT() * 1000) {
        manager.packetDropped();
        return -1;
    }
    nextSendTime_ = finishTime;

    if (unit_(rng_) < options_.getLossRate()) {
        manager.packetLost();
        return -1;
    }
    return finishTime + (options_.getDelay() * 1000);
}

} // namespace nb
