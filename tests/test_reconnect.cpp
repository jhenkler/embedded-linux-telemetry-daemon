#include <gtest/gtest.h>
#include <chrono>

#include "mqtt_client.h"

using namespace std::chrono_literals;

TEST(MqttClientReconnect, in_flight_timeout_clears_gate_and_retries) {
    #ifdef UNIT_TESTS
        MqttClient mqtt("host", 1883, "client_id", 0);

        mqtt.set_mosq_present_for_test(true);

        TimePoint now = Clock::now();
        mqtt.set_now_fn_for_test([&]{ return now; });

        int calls = 0;
        mqtt.set_reconnect_fn_for_test([&] {
            ++calls;
            return MOSQ_ERR_SUCCESS;
        });

        // first attempt -> in flight
        mqtt.tick();
        EXPECT_EQ(calls, 1);

        // still in flight -> no new attempt
        mqtt.tick();
        EXPECT_EQ(calls, 1);

        // Advance past timeout (15s)
        now += (MqttClient::reconnect_in_flight_timeout_for_test() + 100s);

        mqtt.tick();
        EXPECT_EQ(calls, 1);

        now = mqtt.backoff_next_time_for_test() + 2s;

        // should timeout in-flight and try again
        mqtt.tick();
        EXPECT_EQ(calls, 2);
    #else 
        GTEST_SKIP() << "Compile with -DUNIT_TESTS to enable test hooks.";
    #endif
}

TEST(MqttClientReconnect, on_connect_resets_backoff_and_clears_in_flight) {
    #ifdef UNIT_TESTS
    MqttClient mqtt("host", 1883, "client_id", 0);
    mqtt.set_mosq_present_for_test(true);

    TimePoint now = Clock::now();
    mqtt.set_now_fn_for_test([&] { return now; });

    int calls = 0;
    mqtt.set_reconnect_fn_for_test([&] { ++calls; return MOSQ_ERR_SUCCESS; });

    mqtt.tick();
    EXPECT_EQ(calls, 1);

    // simulate successful connect callback
    mqtt.simulate_connect_for_test(0);

    // simulate disconnect
    mqtt.simulate_disconnect_for_test(1);

    mqtt.tick();
    EXPECT_EQ(calls, 2);

    #else
        GTEST_SKIP() << "Compile with -DUNIT_TESTS to enable test hooks.";
    #endif
}