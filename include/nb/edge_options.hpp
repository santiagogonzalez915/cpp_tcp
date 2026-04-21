#pragma once

#include <cstdint>

namespace nb {

struct EdgeOptions {
    double lossRate = 0.0;
    std::int64_t delayMs = 1;
    int bwBytesPerSec = 10000;
    std::int64_t bufferingMs = 250;

    void setLossRate(double r) { lossRate = r; }
    void setDelay(std::int64_t d) { delayMs = d; }
    void setBW(int bw) { bwBytesPerSec = bw; }
    void setBT(std::int64_t bt) { bufferingMs = bt; }

    double getLossRate() const { return lossRate; }
    std::int64_t getDelay() const { return delayMs; }
    int getBW() const { return bwBytesPerSec; }
    std::int64_t getBT() const { return bufferingMs; }
};

} // namespace nb
