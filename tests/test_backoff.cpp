#include <gtest/gtest.h>
#include <chrono>

#include "reconnect_backoff.h"

struct FakeClock {
    ReconnectBackoff::TimePoint now = ReconnectBackoff::Clock::now();

    void advance(std::chrono::seconds secs) { now += secs; }
}; 
 
TEST(ReconnectBackoffTest, initial_attempt_is_immediate) { 
    FakeClock fake_clock; 
    ReconnectBackoff rc_backoff;

    EXPECT_TRUE(rc_backoff.can_attempt(fake_clock.now));
}

TEST(ReconnectBackoffTest, failure_doubles_backoff) {
    FakeClock fake_clock;
    ReconnectBackoff rc_backoff(1, 60, [] { return std::chrono::milliseconds(0); });

    rc_backoff.schedule_attempt(fake_clock.now);
    EXPECT_EQ(rc_backoff.backoff_seconds(), 2);

    fake_clock.advance(std::chrono::seconds(1));
    EXPECT_TRUE(rc_backoff.can_attempt(fake_clock.now));

    rc_backoff.schedule_attempt(fake_clock.now);
    EXPECT_EQ(rc_backoff.backoff_seconds(), 4);

    fake_clock.advance(std::chrono::seconds(1));
    EXPECT_FALSE(rc_backoff.can_attempt(fake_clock.now));

    fake_clock.advance(std::chrono::seconds(1));
    EXPECT_TRUE(rc_backoff.can_attempt(fake_clock.now));
}

TEST(ReconnectBackoffTest, caps_at_max) {
    FakeClock fake_clock;
    ReconnectBackoff rc_backoff(1 /*initial_seconds*/, 8 /*max_seconds*/, [] { return std::chrono::milliseconds(0); });

    for (int i = 0; i < 10; ++i) {
        rc_backoff.schedule_attempt(fake_clock.now);
        fake_clock.now = rc_backoff.next_time();
    }
    EXPECT_EQ(rc_backoff.backoff_seconds(), 8);
}