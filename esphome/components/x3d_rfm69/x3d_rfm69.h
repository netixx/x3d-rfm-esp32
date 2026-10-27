#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "x3d_frame.h"

namespace esphome::x3d_rfm69 {

/// Gets every handle position of one X3D device, e.g. the read-only lock
/// entity, which lives in its own platform so the hub doesn't need `lock`.
class HandleListener {
 public:
  virtual void on_handle(HandleState state, bool tilted) = 0;
  /// No frame for longer than the timeout: the position is unknown.
  virtual void on_timeout() {}
};

/// Entities of one X3D device (one sash of a window or door), grouped by the hub.
class X3DOpening {
 public:
  X3DOpening(uint32_t x3d_id, const char *name) : x3d_id_(x3d_id), name_(name) {}

  void set_lock_sensor(binary_sensor::BinarySensor *sensor) { this->lock_ = sensor; }
  void set_tilt_sensor(binary_sensor::BinarySensor *sensor) { this->tilt_ = sensor; }
  void set_state_sensor(text_sensor::TextSensor *sensor) { this->state_ = sensor; }
  void set_rssi_sensor(sensor::Sensor *sensor) { this->rssi_ = sensor; }
  void set_connectivity_sensor(binary_sensor::BinarySensor *sensor) { this->connectivity_ = sensor; }
  void add_handle_listener(HandleListener *listener) { this->listeners_.push_back(listener); }

  uint32_t get_x3d_id() const { return this->x3d_id_; }
  const char *get_name() const { return this->name_; }

  /// Publish the last handle position saved in flash, so entities aren't
  /// "unknown" after a reboot until the handle moves again.
  void restore_state() {
    this->pref_ = global_preferences->make_preference<uint8_t>(fnv1_hash_extend(fnv1_hash("x3d_rfm69"),
                                                                                this->x3d_id_));
    uint8_t saved;
    if (this->pref_.load(&saved) && saved >= SAVED_LOCKED && saved <= SAVED_TILTED) {
      this->saved_ = saved;
      this->publish_handle_(saved == SAVED_LOCKED ? HandleState::LOCKED : HandleState::UNLOCKED, saved == SAVED_TILTED);
    }
    this->restore_ = true;
  }

  /// Handle event (sensor message).
  void publish(const SensorFrame &frame, float rssi) {
    this->heard_(!has_handle_position(frame.state));
    if (has_handle_position(frame.state)) {
      const bool tilted = is_tilted(frame);
      this->publish_handle_(frame.state, tilted);
      const uint8_t saved =
          frame.state == HandleState::LOCKED ? SAVED_LOCKED : (tilted ? SAVED_TILTED : SAVED_UNLOCKED);
      if (this->restore_ && saved != this->saved_) {
        this->saved_ = saved;
        this->pref_.save(&saved);
      }
    }
    this->publish_rssi_(rssi);
  }

  /// Any valid frame from this device: only the signal strength is known.
  void publish_seen(float rssi) {
    this->heard_(true);
    this->publish_rssi_(rssi);
  }

  /// Called periodically with the hub timeout (0 = off). Returns true when the
  /// device just went silent: its entities are cleared ("unknown" in HA).
  bool check_timeout(uint32_t now, uint32_t timeout_ms) {
    if (timeout_ms == 0 || this->timed_out_ || now - this->last_seen_ms_ < timeout_ms)
      return false;
    this->timed_out_ = true;
    if (this->lock_ != nullptr)
      this->lock_->invalidate_state();
    if (this->tilt_ != nullptr)
      this->tilt_->invalidate_state();
    if (this->state_ != nullptr)
      this->state_->publish_state("unknown");
    if (this->rssi_ != nullptr)
      this->rssi_->publish_state(NAN);
    if (this->connectivity_ != nullptr)
      this->connectivity_->publish_state(false);
    for (auto *listener : this->listeners_)
      listener->on_timeout();
    return true;
  }
  uint32_t get_last_seen_ms() const { return this->last_seen_ms_; }
  /// Start of the timeout window (boot).
  void reset_last_seen(uint32_t now) { this->last_seen_ms_ = now; }

 protected:
  static constexpr uint8_t SAVED_LOCKED = 1;
  static constexpr uint8_t SAVED_UNLOCKED = 2;
  static constexpr uint8_t SAVED_TILTED = 3;

  void publish_rssi_(float rssi) {
    if (this->rssi_ != nullptr)
      this->rssi_->publish_state(rssi);
  }

  /// A frame arrived. After a timeout, publish the last known position again
  /// (unless the frame brings a new one): the handle hasn't moved since.
  void heard_(bool republish_last) {
    this->last_seen_ms_ = millis();
    if (this->connectivity_ != nullptr)
      this->connectivity_->publish_state(true);
    if (this->timed_out_) {
      this->timed_out_ = false;
      if (republish_last && has_handle_position(this->last_state_))
        this->publish_handle_(this->last_state_, this->last_tilted_);
    }
  }

  void publish_handle_(HandleState state, bool tilted) {
    this->last_state_ = state;
    this->last_tilted_ = tilted;
    // Home Assistant lock device class: ON means unlocked.
    if (this->lock_ != nullptr)
      this->lock_->publish_state(state == HandleState::UNLOCKED);
    if (this->tilt_ != nullptr)
      this->tilt_->publish_state(tilted);
    if (this->state_ != nullptr)
      this->state_->publish_state(handle_state_to_string(state));
    for (auto *listener : this->listeners_)
      listener->on_handle(state, tilted);
  }

  uint32_t x3d_id_;
  const char *name_;
  ESPPreferenceObject pref_;
  uint8_t saved_{0};
  bool restore_{false};
  binary_sensor::BinarySensor *lock_{nullptr};
  binary_sensor::BinarySensor *tilt_{nullptr};
  text_sensor::TextSensor *state_{nullptr};
  sensor::Sensor *rssi_{nullptr};
  binary_sensor::BinarySensor *connectivity_{nullptr};
  std::vector<HandleListener *> listeners_;
  uint32_t last_seen_ms_{0};
  bool timed_out_{false};
  HandleState last_state_{HandleState::UNCHANGED};
  bool last_tilted_{false};
};

class X3DRFM69 : public Component,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_1MHZ> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_dio0_pin(InternalGPIOPin *pin) { this->dio0_pin_ = pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_log_unclaimed_devices(bool log) { this->log_unclaimed_devices_ = log; }
  void set_restore_state(bool restore) { this->restore_state_ = restore; }
  void set_timeout(uint32_t timeout_ms) { this->timeout_ms_ = timeout_ms; }

  /// `platform: x3d_rfm69` entities: attached to the opening of their X3D
  /// device, created on first use.
  void add_lock_sensor(uint32_t x3d_id, binary_sensor::BinarySensor *sensor) {
    this->opening_for_(x3d_id, sensor)->set_lock_sensor(sensor);
  }
  void add_tilt_sensor(uint32_t x3d_id, binary_sensor::BinarySensor *sensor) {
    this->opening_for_(x3d_id, sensor)->set_tilt_sensor(sensor);
  }
  void add_state_sensor(uint32_t x3d_id, text_sensor::TextSensor *sensor) {
    this->opening_for_(x3d_id, sensor)->set_state_sensor(sensor);
  }
  void add_rssi_sensor(uint32_t x3d_id, sensor::Sensor *sensor) {
    this->opening_for_(x3d_id, sensor)->set_rssi_sensor(sensor);
  }
  void add_connectivity_sensor(uint32_t x3d_id, binary_sensor::BinarySensor *sensor) {
    this->opening_for_(x3d_id, sensor)->set_connectivity_sensor(sensor);
  }
  void add_handle_listener(uint32_t x3d_id, EntityBase *entity, HandleListener *listener) {
    this->opening_for_(x3d_id, entity)->add_handle_listener(listener);
  }
  /// Hub-wide text sensor showing the last event of any device (discovery).
  void set_last_frame_sensor(text_sensor::TextSensor *sensor) { this->last_frame_ = sensor; }
  /// (device_id hex string, state, tilted, kind, rssi, raw frame hex), once per new event of any device.
  Trigger<std::string, std::string, bool, std::string, float, std::string> *get_opening_event_trigger() {
    return &this->event_trigger_;
  }

 protected:
  uint8_t read_register_(uint8_t reg);
  void write_register_(uint8_t reg, uint8_t value);
  void set_mode_(uint8_t mode);
  void configure_radio_();
  void receive_begin_();
  bool payload_ready_();
  void read_and_handle_packet_();
  void handle_frame_(const uint8_t *buf, size_t len, float rssi);
  X3DOpening *find_opening_(uint32_t x3d_id);
  X3DOpening *opening_for_(uint32_t x3d_id, EntityBase *entity);
  /// True the first time an unclaimed device is seen since boot.
  bool first_sighting_(uint32_t x3d_id);
  /// True the first time a device sends these unexpected bytes 8-14 since boot.
  bool first_unexpected_(const SensorFrame &frame);
  /// Warn once if an opening was configured with the ID in raw (byte-reversed) order.
  void check_reversed_id_(uint32_t x3d_id, const char *id_str);

  static void gpio_intr(X3DRFM69 *arg);

  InternalGPIOPin *dio0_pin_{nullptr};
  uint32_t frequency_{868950000};
  bool log_unclaimed_devices_{true};
  bool restore_state_{true};
  uint32_t timeout_ms_{0};
  uint8_t mode_{0xff};

  Deduplicator dedup_;
  std::vector<X3DOpening *> openings_;
  std::vector<uint32_t> sighted_;
  std::vector<uint32_t> warned_;
  text_sensor::TextSensor *last_frame_{nullptr};
  Trigger<std::string, std::string, bool, std::string, float, std::string> event_trigger_;
};

}  // namespace esphome::x3d_rfm69
