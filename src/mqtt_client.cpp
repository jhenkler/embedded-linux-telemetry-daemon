#include <string>
#include <mosquitto.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <algorithm>
#include <random>

#include "mqtt_client.h"
#include "logger.h"
#include "topic_builder.h"
#include "status_payload.h"

MqttClient::MqttClient(std::string host, int port, std::string client_id, int qos) 
    : host_(std::move(host)), 
      port_(port), 
      client_id_(std::move(client_id)), 
      qos_(qos),
      rng_(std::random_device{}())
    {

        // clean_session=true, userdata=this
        mosq_ = mosquitto_new(client_id_.c_str(), true, this);
        if (!mosq_) {
            LOG_ERROR("mosquitto_new failed");
            return;
        }

        // Call backs
        mosquitto_connect_callback_set(mosq_, &MqttClient::on_connect);
        mosquitto_disconnect_callback_set(mosq_, &MqttClient::on_disconnect);

        // LWT
        setup_lwt_();
    }

MqttClient::~MqttClient() {
    stop();
    if (mosq_) {
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
    }
}

bool MqttClient::has_mosq_() const noexcept {
    #ifdef UNIT_TESTS
        if (mosq_present_for_test_.load(std::memory_order_relaxed)) return true;
    #endif
        return mosq_ != nullptr;
}

void MqttClient::on_connect(struct mosquitto* /*mosq*/, void* obj, int rc) {
    auto* self = static_cast<MqttClient*>(obj);

    if (rc == 0) {
        self->connected_.store(true, std::memory_order_relaxed);
        self->reconnect_in_flight_.store(false, std::memory_order_relaxed);
        self->reconnect_started_ticks_.store(0, std::memory_order_relaxed);
        self->backoff_.reset();

        LOG_INFO("Connected to broker");
        // mark online (retained)
        self->publish_status_(self->online_payload_);
    } else {
        self->connected_.store(false, std::memory_order_relaxed);
        LOG_ERROR("Connect failed rc=" + std::to_string(rc));
    }
}

void MqttClient::on_disconnect(struct mosquitto* /*mosq*/, void* obj, int rc) {
    auto* self = static_cast<MqttClient*>(obj);

    self->connected_.store(false, std::memory_order_relaxed);
    self->reconnect_in_flight_.store(false, std::memory_order_relaxed);
    self->reconnect_started_ticks_.store(0, std::memory_order_relaxed);

    if (self->stopping_.load(std::memory_order_relaxed)) {
        LOG_INFO("Disconnected cleanly rc=" + std::to_string(rc));
    } else {
        LOG_WARN("Disconnect rc=" + std::to_string(rc) + " (will reconnect)");
    }
}

bool MqttClient::connect(int keepalive_seconds) {
    if (!has_mosq_()) return false;

    int rc = mosquitto_connect_async(mosq_, host_.c_str(), port_, keepalive_seconds);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR(std::string("mosquitto_connect_async error: ") + mosquitto_strerror(rc));
        return false;
    }

    bool expected = false;
    if (loop_started_.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        rc = mosquitto_loop_start(mosq_);
        if ( rc != MOSQ_ERR_SUCCESS) {
            LOG_ERROR(std::string("mosquitto_loop_start error: ") + mosquitto_strerror(rc));
            loop_started_.store(false, std::memory_order_relaxed);
            return false;
        }
    }

    return true;
}

void MqttClient::tick() { tick_reconnect_(); }

void MqttClient::tick_reconnect_() {
    if (stopping_.load(std::memory_order_relaxed)) return;
    if (connected_.load(std::memory_order_relaxed)) return;
    if (!has_mosq_()) return;

    const auto now = now_fn_();

    // if attempt is stuck, clear in-flight so we can try again
    if (in_flight_timed_out_(now)) { 
        LOG_WARN("Reconnect attempt timed out. Allowing another attempt"); 
        backoff_.schedule_attempt(now);
    }
    if (reconnect_in_flight_.load(std::memory_order_relaxed)) return;

    if (!backoff_.can_attempt(now)) return;

    int rc = reconnect_fn_();
    if (rc == MOSQ_ERR_SUCCESS) {
        reconnects_.fetch_add(1, std::memory_order_relaxed);
        reconnect_started_ticks_.store(to_ticks_(now), std::memory_order_relaxed);
        reconnect_in_flight_.store(true, std::memory_order_relaxed);
    } else {
        LOG_ERROR(std::string("reconnect_async error: ") + mosquitto_strerror(rc));
    }

    // Always schedule next attempt
    backoff_.schedule_attempt(now);
}

bool MqttClient::in_flight_timed_out_(TimePoint now) noexcept {
    if (!reconnect_in_flight_.load(std::memory_order_relaxed)) {
        return false;
    }

    // incase start time never got recorded
    const int64_t started_ticks = reconnect_started_ticks_.load(std::memory_order_relaxed);
    if (started_ticks == 0) {
        reconnect_in_flight_.store(false, std::memory_order_relaxed);
        return true;
    }

    const TimePoint started{TimePoint::duration{started_ticks}};
    if (now - started >= k_reconnect_in_flight_timeout) {
        reconnect_in_flight_.store(false, std::memory_order_relaxed);
        reconnect_started_ticks_.store(0, std::memory_order_relaxed);
        return true;
    }

    return false;
}

bool MqttClient::ensure_connected() {
    if (connected_.load(std::memory_order_relaxed)) return true;
    tick_reconnect_();
    return connected_.load(std::memory_order_relaxed);
}

bool MqttClient::publish(std::string_view topic, std::string_view payload, int qos, bool retain) {
    if (!ensure_connected()) return false;

    int payload_len = static_cast<int>(payload.size());
    std::string topic_str(topic);
    int rc = mosquitto_publish(
        mosq_,
        nullptr,
        topic_str.c_str(),
        payload_len,
        payload.data(),
        qos,
        retain
    );

    if (rc == MOSQ_ERR_NO_CONN) {
        connected_.store(false, std::memory_order_relaxed);
        tick_reconnect_();
        return false;
    }

    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR(std::string("mosquitto publish error: ") + mosquitto_strerror(rc));
        return false;
    }
    return true;
}

void MqttClient::stop() noexcept {
    if (stopping_.exchange(true, std::memory_order_relaxed)) return;
    if (!has_mosq_()) return;

    // mark offline (retained)
    publish_status_(will_payload_);

    mosquitto_disconnect(mosq_);

    if (loop_started_.load(std::memory_order_relaxed)) {
        mosquitto_loop_stop(mosq_, true);
        loop_started_.store(false, std::memory_order_relaxed);
    }
}

// status/LWT
void MqttClient::setup_lwt_() {
    status_topic_ = make_status_topic(client_id_);

    will_payload_   = make_status_payload_v1(client_id_, "offline").dump();
    online_payload_ = make_status_payload_v1(client_id_, "online").dump();

    const bool retain = true;

    int rc = mosquitto_will_set(
        mosq_,
        status_topic_.c_str(),
        static_cast<int>(will_payload_.size()),
        will_payload_.data(),
        qos_,
        retain
    );

    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_WARN(std::string("mosquitto_will_set failed: ") + mosquitto_strerror(rc));
    }
}

void MqttClient::publish_status_(const std::string& payload) {
    if (!has_mosq_()) return;
    if (!connected_.load(std::memory_order_relaxed)) return;
    const bool retain = true;

    int rc = mosquitto_publish(
        mosq_,
        nullptr,
        status_topic_.c_str(),
        static_cast<int>(payload.size()),
        payload.data(),
        qos_,
        retain
    );

    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_DEBUG(std::string("status publish failed: ") + mosquitto_strerror(rc));
    }
}
