#pragma once

#include <chrono>
#include <algorithm>
#include <functional>

class ReconnectBackoff {
    public:
        using Clock     = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        explicit ReconnectBackoff (
            int initial_seconds = 1,
            int max_seconds = 60,
            std::function<std::chrono::milliseconds()> jitter_fn = [] { return std::chrono::milliseconds(0); }
        )
            : initial_(initial_seconds),
            max_(max_seconds),
            backoff_(initial_seconds),
            jitter_fn_(std::move(jitter_fn))
        {}
    
        bool can_attempt(TimePoint now) const {
            return next_.time_since_epoch().count() == 0 || now >= next_;
        }

        void schedule_attempt(TimePoint now) { schedule_next(now); }

        void reset() {
            backoff_ = initial_;
            next_ = {};
        }

        int backoff_seconds() const { return backoff_; }
        TimePoint next_time() const { return next_; };

    private:
        void schedule_next(TimePoint now) {
            auto delay = std::chrono::seconds(backoff_) + jitter_fn_();
            next_ = now + delay;
            backoff_ = std::min(backoff_ * 2, max_);
        }

        int initial_;
        int max_;
        int backoff_{1};
        TimePoint next_{};

        std::function<std::chrono::milliseconds()> jitter_fn_;
};