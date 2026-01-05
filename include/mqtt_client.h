#pragma once

#include <mosquitto.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <random>
#include <cstdint>
#include <functional>

#include "reconnect_backoff.h"

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

class MqttClient {
    public:
        MqttClient(std::string host, int port, std::string client_id, int qos);
        ~MqttClient();

        MqttClient(const MqttClient&) = delete;
        MqttClient& operator = (const MqttClient&) = delete;
        
        bool connect(int keepalive_seconds = 60);
        void tick(); // pulse (non-blocking reconnect attempts)
        std::uint64_t reconnects() const noexcept { return reconnects_.load(std::memory_order_relaxed); }
        
        bool publish(std::string_view topic, std::string_view payload, int qos = 0, bool retain = false);

        void stop() noexcept;

        const std::string& client_id() const { return client_id_; };

    private:
        // ----common variables----
        std::string host_;
        int port_;
        std::string client_id_;
        mosquitto* mosq_ = nullptr;

        bool has_mosq_() const noexcept;

        // ----connection----
        std::atomic<bool> connected_ {false};
        std::atomic<bool> stopping_ {false};
        std::atomic<bool> loop_started_ {false};;

        static void on_connect(struct mosquitto* mosq, void* obj, int rc);
        static void on_disconnect(struct mosquitto* mosq, void* obj, int rc);
        bool ensure_connected();

        // ----reconnect----
        // flags
        std::atomic<bool> reconnect_in_flight_{false};

        // in-flight timeout tracking
        std::atomic<int64_t> reconnect_started_ticks_{0};
        static constexpr auto k_reconnect_in_flight_timeout = std::chrono::seconds(15);
        bool in_flight_timed_out_(TimePoint now) noexcept;

        // helper to convert Timepoint -> int64 ticks
        static int64_t to_ticks_(TimePoint tp) noexcept { return tp.time_since_epoch().count(); }

        // consts
        static constexpr int k_max_backoff_seconds = 60;

        // counters
        std::atomic<uint64_t> reconnects_ {0};

        std::function<int()> reconnect_fn_ = [this] { return mosquitto_reconnect_async(mosq_); };

        void tick_reconnect_();

        // ----status/LWT----
        std::string status_topic_;
        std::string will_payload_; // offline
        std::string online_payload_; // online
        int qos_;

        void setup_lwt_();
        void publish_status_(const std::string& payload);

        // ----time functions----
        std::function<TimePoint()> now_fn_ = [] { return Clock::now(); };

        // ----jitter----
        std::mt19937 rng_;
        std::uniform_int_distribution<int> jitter_dist_{0, 200};

        // ----backoff----
        ReconnectBackoff backoff_{
            1,
            k_max_backoff_seconds,
            [this] { return std::chrono::milliseconds(jitter_dist_(rng_)); }
        };

    #ifdef UNIT_TESTS
    private:
        std::atomic<bool> mosq_present_for_test_{false};

    public:

        void set_now_fn_for_test(std::function<TimePoint()> func) { now_fn_ = std::move(func); }
        void set_reconnect_fn_for_test(std::function<int()> func) { reconnect_fn_ = std::move(func); }
        void set_mosq_present_for_test(bool present) { 
            mosq_present_for_test_.store(present, std::memory_order_relaxed); 
        }
        void simulate_connect_for_test(int rc) { MqttClient::on_connect(nullptr, this, rc); }
        void simulate_disconnect_for_test(int rc) { MqttClient::on_disconnect(nullptr, this, rc); } 
        static constexpr auto reconnect_in_flight_timeout_for_test() noexcept {
            return k_reconnect_in_flight_timeout;
        }
    #endif
};