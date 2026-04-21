#include "nb/ordered_event_queue.hpp"

#include <gtest/gtest.h>

using namespace nb;

TEST(OrderedEventQueueTest, InsertSorted) {
    OrderedEventQueue q;
    int order = 0;
    q.addEvent(ScheduledEvent(300, [&] { order += 3; }));
    q.addEvent(ScheduledEvent(100, [&] { order = order * 10 + 1; }));
    q.addEvent(ScheduledEvent(200, [&] { order = order * 10 + 2; }));

    ASSERT_EQ(q.size(), 3u);
    auto ev1 = q.removeNextEvent(); ev1.callback();
    auto ev2 = q.removeNextEvent(); ev2.callback();
    auto ev3 = q.removeNextEvent(); ev3.callback();
    EXPECT_EQ(order, 15);
    EXPECT_TRUE(q.empty());
}

TEST(OrderedEventQueueTest, EqualTimesAppendedInOrder) {
    OrderedEventQueue q;
    std::string s;
    q.addEvent(ScheduledEvent(100, [&] { s += "a"; }));
    q.addEvent(ScheduledEvent(100, [&] { s += "b"; }));
    q.addEvent(ScheduledEvent(100, [&] { s += "c"; }));
    while (!q.empty()) {
        auto ev = q.removeNextEvent();
        ev.callback();
    }
    EXPECT_EQ(s, "abc");
}
