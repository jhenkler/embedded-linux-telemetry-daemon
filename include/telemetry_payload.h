#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <chrono>
#include <cstdint>

#include "time_utils.h"

inline nlohmann::json make_payload_v1(
    std::string_view client_id,
    std::string_view metric_name,
    std::string_view unit,
    double value,
    std::uint64_t seq
) {
    return {
        {"schema_version", 1},
        {"device", {{"client_id", client_id}}},
        {"metric", {{"name", metric_name}, {"unit", unit}, {"value", value}}},
        {"timestamp_s", unix_time_s()},
        {"seq", seq}
    };
}