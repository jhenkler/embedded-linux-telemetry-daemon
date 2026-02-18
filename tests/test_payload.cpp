#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <cstdint>

#include "telemetry_payload.h"
#include "health_payload.h"
#include "status_payload.h"

TEST(Telemetry_Payload_V1, has_version_and_field) {
    std::string client_id = "pi-sim-01";
    std::string metric_name = "temperature";
    std::string unit = "C";
    double value = 25.5;
    std::uint64_t seq = 3;

    auto payload = make_payload_v1(client_id, metric_name, unit, value, seq);

    EXPECT_TRUE(payload.contains("schema_version"));
    EXPECT_TRUE(payload.contains("device"));
    EXPECT_TRUE(payload.contains("metric"));
    EXPECT_TRUE(payload["device"].contains("client_id"));
    EXPECT_TRUE(payload["metric"].contains("name"));
    EXPECT_TRUE(payload["metric"].contains("unit"));
    EXPECT_TRUE(payload["metric"].contains("value"));
    EXPECT_TRUE(payload.contains("timestamp_s"));
    EXPECT_TRUE(payload.contains("seq"));

    EXPECT_EQ(payload["schema_version"], 1);
    EXPECT_EQ(payload["device"]["client_id"], client_id);
    EXPECT_EQ(payload["metric"]["name"], metric_name);
    EXPECT_EQ(payload["metric"]["unit"], unit);
    EXPECT_EQ(payload["metric"]["value"], value);
    EXPECT_EQ(payload["seq"], seq);
}

TEST(Health_payload_V1, has_version_and_field) {
    std::string client_id = "pi-sim-01";
    std::uint64_t uptime_s = 100;
    std::uint64_t seq = 7;
    std::uint64_t publish_ok = 1;
    std::uint64_t publish_fail = 0;
    std::uint64_t reconnects = 77;
    std::uint64_t now_s = 77777777;

    auto payload = make_health_payload_v1(client_id, uptime_s, seq, publish_ok, publish_fail, reconnects, now_s);

    EXPECT_TRUE(payload.contains("schema_version"));
    EXPECT_TRUE(payload["device"].contains("client_id"));
    EXPECT_TRUE(payload.contains("uptime_s"));
    EXPECT_TRUE(payload.contains("seq"));
    EXPECT_TRUE(payload["counters"].contains("publish_ok"));
    EXPECT_TRUE(payload["counters"].contains("publish_fail"));
    EXPECT_TRUE(payload["counters"].contains("reconnects"));
    EXPECT_TRUE(payload.contains("timestamp_s"));

    EXPECT_EQ(payload["schema_version"], 1);
    EXPECT_EQ(payload["device"]["client_id"], client_id);
    EXPECT_EQ(payload["uptime_s"], uptime_s);
    EXPECT_EQ(payload["seq"], seq);
    EXPECT_EQ(payload["counters"]["publish_ok"], publish_ok);
    EXPECT_EQ(payload["counters"]["publish_fail"], publish_fail);
    EXPECT_EQ(payload["counters"]["reconnects"], reconnects);
    EXPECT_EQ(payload["timestamp_s"], now_s);
}