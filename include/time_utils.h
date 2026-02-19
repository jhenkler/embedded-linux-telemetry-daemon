#pragma once

#include <chrono>
#include <cstdint>

inline std::int64_t unix_time_s() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}