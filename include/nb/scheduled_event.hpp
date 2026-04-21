#pragma once

#include <cstdint>
#include <functional>
#include <utility>

namespace nb {

using TimerCallback = std::function<void()>;

// ScheduledEvent: a (time, callback) pair scheduled for future execution.
// time is an absolute wall-clock time in microseconds for the emulator, or
// an absolute simulated time in microseconds for the simulator.
struct ScheduledEvent {
    std::int64_t timeToOccur = 0;
    TimerCallback callback;

    ScheduledEvent() = default;
    ScheduledEvent(std::int64_t t, TimerCallback cb)
        : timeToOccur(t), callback(std::move(cb)) {}
};

} // namespace nb
