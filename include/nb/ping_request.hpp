#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nb {

struct PingRequest {
    int destAddr;
    std::vector<std::uint8_t> msg;
    std::int64_t timeSentMs;

    PingRequest(int d, std::vector<std::uint8_t> m, std::int64_t t)
        : destAddr(d), msg(std::move(m)), timeSentMs(t) {}
};

} // namespace nb
