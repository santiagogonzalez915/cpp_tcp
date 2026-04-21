#include "nb/ordered_event_queue.hpp"

#include <utility>

namespace nb {

void OrderedEventQueue::addEvent(ScheduledEvent event) {
    for (auto it = events_.begin(); it != events_.end(); ++it) {
        if (event.timeToOccur < it->timeToOccur) {
            events_.insert(it, std::move(event));
            return;
        }
    }
    events_.push_back(std::move(event));
}

const ScheduledEvent* OrderedEventQueue::getNextEvent() const {
    if (events_.empty()) return nullptr;
    return &events_.front();
}

ScheduledEvent OrderedEventQueue::removeNextEvent() {
    ScheduledEvent ev = std::move(events_.front());
    events_.pop_front();
    return ev;
}

} // namespace nb
