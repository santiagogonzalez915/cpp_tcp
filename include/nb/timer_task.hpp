#pragma once

#include <cstdint>
#include <memory>

namespace nb {

class RuntimeManager;
class StackNode;

// TimerTask: mirror of Java FishThread.java. A periodic callback driven by
// the Manager's timer. Not a real OS thread — just a recurring event.
class TimerTask : public std::enable_shared_from_this<TimerTask> {
public:
    static constexpr long DEFAULT_INTERVAL_MS = 1000;

    TimerTask(RuntimeManager* manager, StackNode* node,
              long intervalMs = DEFAULT_INTERVAL_MS);
    virtual ~TimerTask() = default;

    void setInterval(long intervalMs) {
        if (intervalMs > 0) intervalMs_ = intervalMs;
    }

    // Kick off the task (schedule the first tick).
    void start();
    // Run once immediately and then schedule.
    void startNow();
    // Stop rescheduling.
    void stop() { intervalMs_ = 0; }

    // Subclass-provided task body (default no-op).
    virtual void execute() {}

protected:
    RuntimeManager* manager_;
    StackNode* node_;
    int addr_;
    long intervalMs_;

private:
    void tick();
    void schedule();
};

} // namespace nb
