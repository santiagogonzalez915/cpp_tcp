#pragma once

#include "nb/edge_options.hpp"

#include <cstdint>
#include <random>

namespace nb {

class RuntimeManager;

// EmulatedLink: bandwidth / buffer / loss pacer on the emulator side,
// mirroring Java EmulatedLink.schedulePkt().
class EmulatedLink {
public:
    explicit EmulatedLink(const EdgeOptions& options);

    std::int64_t schedulePkt(RuntimeManager& manager, int size,
                             std::int64_t nowMicros);

private:
    EdgeOptions options_;
    std::int64_t nextSendTime_ = 0;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> unit_;
};

} // namespace nb
