#pragma once

#include <chrono>
#include <cstdint>

namespace nb {

// Wall-clock microseconds since epoch, matching Java Utility.fishTime()
// which returns System.currentTimeMillis() * 1000 (ms granularity, us units).
inline std::int64_t clockMicros() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch()).count() * 1000;
}

} // namespace nb
