#include <gtest/gtest.h>
#include <string>

#include "topic_builder.h"

TEST(Topics, telemetry_topic_layout) {
    std::string client_id = "pi-sim-01";
    std::string suffix = "temp";

    auto topic = make_topic(client_id, suffix);
    EXPECT_EQ(topic, "devices/pi-sim-01-temp");
}

TEST(Topics, status_topic_layout) {
    std::string client_id = "pi-sim-01";

    auto topic = make_status_topic(client_id);
    EXPECT_EQ(topic, "devices/pi-sim-01/status");
}

TEST(Topics, health_topic_layout) {
    std::string client_id = "pi-sim-01";

    auto topic = make_health_topic(client_id);
    EXPECT_EQ(topic, "devices/pi-sim-01/health");
}