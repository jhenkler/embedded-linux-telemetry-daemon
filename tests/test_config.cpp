#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "app_config.h"

TEST(Config, reject_negative_interval) {
    nlohmann::json jsn = {
        {"log_level", "info"},
        {"broker", {{"host", "localhost"}, {"port", 1883}, {"keepalive", 10}}},
        {"client_id", "pi-sim-01"},
        {"interval_ms", -1000},
        {"qos", 1},
        {"retain", false},
        {"metrics", nlohmann::json::array({ {{"name", "temperature"}, {"unit", "C"}, {"start", 20.0}, {"step", 0.25}, {"topic_suffix", "temp"}} })}
    };
    EXPECT_THROW(parse_config_or_throw(jsn), std::runtime_error);
    SUCCEED();
}

TEST(Config, applies_defaults) {
    nlohmann::json jsn = {
        {"log_level", "infofd"},
        //{"broker", {{"host", "localhost"}, {"port", 1883}, {"keepalive", 10}}},
        {"client_id", "pi-sim-01"},
        //{"interval_ms", 1000},
        {"qos", 1},
        //{"retain", false},
        {"metrics", nlohmann::json::array({ {{"name", "temperature"}, {"unit", "C"}, {"start", 20.0}, {"step", 0.25}, {"topic_suffix", "temp"}} })}
    };
    auto cfg = parse_config_or_throw(jsn);
    EXPECT_EQ(cfg.log_level, "info");
    EXPECT_EQ(cfg.host, "localhost");
    EXPECT_EQ(cfg.port, 1883);
    EXPECT_EQ(cfg.keepalive_s, 60);
    EXPECT_EQ(cfg.client_id, "pi-sim-01");
    EXPECT_EQ(cfg.interval_ms, 100);
    EXPECT_EQ(cfg.qos, 1);
    EXPECT_EQ(cfg.retain, false);
    SUCCEED();
}

TEST(Config, client_id_empty) {
        nlohmann::json jsn = {
        {"log_level", "info"},
        {"broker", {{"host", "localhost"}, {"port", 1883}, {"keepalive", 10}}},
        {"client_id", ""},
        {"interval_ms", -1000},
        {"qos", 1},
        {"retain", false},
        {"metrics", nlohmann::json::array({ {{"name", "temperature"}, {"unit", "C"}, {"start", 20.0}, {"step", 0.25}, {"topic_suffix", "temp"}} })}
    };
    EXPECT_THROW(parse_config_or_throw(jsn), std::runtime_error);
    SUCCEED();
}

TEST(Config, qos_out_of_range_pos) {
        nlohmann::json jsn = {
        {"log_level", "info"},
        {"broker", {{"host", "localhost"}, {"port", 1883}, {"keepalive", 10}}},
        {"client_id", "pi-sim-01"},
        {"interval_ms", 100},
        {"qos", 7},
        {"retain", false},
        {"metrics", nlohmann::json::array({ {{"name", "temperature"}, {"unit", "C"}, {"start", 20.0}, {"step", 0.25}, {"topic_suffix", "temp"}} })}
    };
    EXPECT_THROW(parse_config_or_throw(jsn), std::runtime_error);
    SUCCEED();
}

TEST(Config, qos_out_of_range_neg) {
            nlohmann::json jsn = {
        {"log_level", "info"},
        {"broker", {{"host", "localhost"}, {"port", 1883}, {"keepalive", 10}}},
        {"client_id", "pi-sim-01"},
        {"interval_ms", 100},
        {"qos", -7},
        {"retain", false},
        {"metrics", nlohmann::json::array({ {{"name", "temperature"}, {"unit", "C"}, {"start", 20.0}, {"step", 0.25}, {"topic_suffix", "temp"}} })}
    };
    EXPECT_THROW(parse_config_or_throw(jsn), std::runtime_error);
    SUCCEED();
}