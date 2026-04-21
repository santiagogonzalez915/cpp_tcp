#pragma once

#include "nb/scheduled_event.hpp"

#include <cstdint>
#include <list>

namespace nb {

// OrderedEventQueue: mirrors Java's SortedEventQueue — a linked list kept
// sorted by timeToOccur, stable insert using strict-less-than (events
// inserted with equal time go to the end of the equal-time run).
class OrderedEventQueue {
public:
    void addEvent(ScheduledEvent event);

    // Return a pointer to the next event without removing it; nullptr if empty.
    const ScheduledEvent* getNextEvent() const;

    // Remove and return the next event. Caller must check empty() first.
    ScheduledEvent removeNextEvent();

    bool empty() const { return events_.empty(); }
    std::size_t size() const { return events_.size(); }

private:
    std::list<ScheduledEvent> events_;
};

} // namespace nb
