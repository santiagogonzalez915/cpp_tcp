#include "nb/timer_task.hpp"

#include "nb/runtime_manager.hpp"
#include "nb/stack_node.hpp"

namespace nb {

TimerTask::TimerTask(RuntimeManager* manager, StackNode* node, long intervalMs)
    : manager_(manager), node_(node), addr_(node->addr()),
      intervalMs_(intervalMs) {}

void TimerTask::start() { schedule(); }

void TimerTask::startNow() {
    execute();
    schedule();
}

void TimerTask::tick() {
    execute();
    schedule();
}

void TimerTask::schedule() {
    if (intervalMs_ <= 0) return;
    auto self = shared_from_this();
    manager_->addTimer(addr_, intervalMs_, [self] { self->tick(); });
}

} // namespace nb
