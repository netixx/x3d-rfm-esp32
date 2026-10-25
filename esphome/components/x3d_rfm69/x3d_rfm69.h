#pragma once

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

class X3DGroup;

/// One window or door declared under `openings:`, owning its entities.
class X3DOpening {
 public:
  X3DOpening(uint32_t device_id, const char *name) : device_id_(device_id), name_(name) {}

  void set_lock_sensor(binary_sensor::BinarySensor *sensor) { this->lock_ = sensor; }
  void set_tilt_sensor(binary_sensor::BinarySensor *sensor) { this->tilt_ = sensor; }
  void set_state_sensor(text_sensor::TextSensor *sensor) { this->state_ = sensor; }
  void set_rssi_sensor(sensor::Sensor *sensor) { this->rssi_ = sensor; }

  void set_group(X3DGroup *group) { this->group_ = group; }

  uint32_t get_device_id() const { return this->device_id_; }
  const char *get_name() const { return this->name_; }
  /// Last known handle position; UNCHANGED until the first event (or restore).
  HandleState get_handle_state() const { return this->handle_; }

  /// Publish the last handle position saved in flash, so entities aren't
  /// "unknown" after a reboot until the handle moves again.
  void restore_state() {
    this->pref_ = global_preferences->make_preference<uint8_t>(fnv1_hash_extend(fnv1_hash("x3d_rfm69"),
                                                                                this->device_id_));
    uint8_t saved;
    if (this->pref_.load(&saved) && saved >= SAVED_LOCKED && saved <= SAVED_TILTED) {
      this->saved_ = saved;
      this->publish_handle_(saved == SAVED_LOCKED ? HandleState::LOCKED : HandleState::UNLOCKED, saved == SAVED_TILTED);
    }
    this->restore_ = true;
  }

  /// Handle event (sensor message).
  void publish(const SensorFrame &frame, float rssi) {
    if (frame.state != HandleState::UNCHANGED) {
      const bool tilted = is_tilted(frame);
      this->publish_handle_(frame.state, tilted);
      const uint8_t saved =
          frame.state == HandleState::LOCKED ? SAVED_LOCKED : (tilted ? SAVED_TILTED : SAVED_UNLOCKED);
      if (this->restore_ && saved != this->saved_) {
        this->saved_ = saved;
        this->pref_.save(&saved);
      }
    }
    this->publish_seen(rssi);
  }

  /// Any valid frame from this device: only the signal strength is known.
  void publish_seen(float rssi) {
    if (this->rssi_ != nullptr)
      this->rssi_->publish_state(rssi);
  }

 protected:
  static constexpr uint8_t SAVED_LOCKED = 1;
  static constexpr uint8_t SAVED_UNLOCKED = 2;
  static constexpr uint8_t SAVED_TILTED = 3;

  void publish_handle_(HandleState state, bool tilted);

  uint32_t device_id_;
  const char *name_;
  ESPPreferenceObject pref_;
  uint8_t saved_{0};
  bool restore_{false};
  HandleState handle_{HandleState::UNCHANGED};
  X3DGroup *group_{nullptr};
  binary_sensor::BinarySensor *lock_{nullptr};
  binary_sensor::BinarySensor *tilt_{nullptr};
  text_sensor::TextSensor *state_{nullptr};
  sensor::Sensor *rssi_{nullptr};
};

/// Openings sharing one sub-device (sashes of a window or door), with a
/// combined lock entity: unlocked if any sash is, locked when all are.
class X3DGroup {
 public:
  void set_lock_sensor(binary_sensor::BinarySensor *sensor) { this->lock_ = sensor; }
  void add_opening(X3DOpening *opening) {
    this->openings_.push_back(opening);
    opening->set_group(this);
  }

  void update() {
    bool all_locked = true;
    for (auto *opening : this->openings_) {
      const HandleState state = opening->get_handle_state();
      if (state == HandleState::UNLOCKED) {
        this->lock_->publish_state(true);  // lock device class: ON = unlocked
        return;
      }
      if (state != HandleState::LOCKED)
        all_locked = false;  // a sash not known yet
    }
    if (all_locked)
      this->lock_->publish_state(false);
  }

 protected:
  binary_sensor::BinarySensor *lock_{nullptr};
  std::vector<X3DOpening *> openings_;
};

inline void X3DOpening::publish_handle_(HandleState state, bool tilted) {
  this->handle_ = state;
  // Home Assistant lock device class: ON means unlocked.
  if (this->lock_ != nullptr)
    this->lock_->publish_state(state == HandleState::UNLOCKED);
  if (this->tilt_ != nullptr)
    this->tilt_->publish_state(tilted);
  if (this->state_ != nullptr)
    this->state_->publish_state(handle_state_to_string(state));
  if (this->group_ != nullptr)
    this->group_->update();
}

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

  void add_opening(X3DOpening *opening) { this->openings_.push_back(opening); }
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
  X3DOpening *find_opening_(uint32_t device_id);
  /// True the first time an unclaimed device is seen since boot.
  bool first_sighting_(uint32_t device_id);
  /// Warn once if an opening was configured with the ID in raw (byte-reversed) order.
  void check_reversed_id_(uint32_t device_id, const char *id_str);

  static void gpio_intr(X3DRFM69 *arg);

  InternalGPIOPin *dio0_pin_{nullptr};
  uint32_t frequency_{868950000};
  bool log_unclaimed_devices_{true};
  bool restore_state_{true};
  uint8_t mode_{0xff};

  Deduplicator dedup_;
  std::vector<X3DOpening *> openings_;
  std::vector<uint32_t> sighted_;
  text_sensor::TextSensor *last_frame_{nullptr};
  Trigger<std::string, std::string, bool, std::string, float, std::string> event_trigger_;
};

}  // namespace esphome::x3d_rfm69
