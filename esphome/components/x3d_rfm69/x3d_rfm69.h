#pragma once

#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "x3d_frame.h"

namespace esphome::x3d_rfm69 {

/// One window or door declared under `openings:`, owning its entities.
class X3DOpening {
 public:
  X3DOpening(uint32_t device_id, const char *name) : device_id_(device_id), name_(name) {}

  void set_lock_sensor(binary_sensor::BinarySensor *sensor) { this->lock_ = sensor; }
  void set_tilt_sensor(binary_sensor::BinarySensor *sensor) { this->tilt_ = sensor; }
  void set_state_sensor(text_sensor::TextSensor *sensor) { this->state_ = sensor; }
  void set_rssi_sensor(sensor::Sensor *sensor) { this->rssi_ = sensor; }

  uint32_t get_device_id() const { return this->device_id_; }
  const char *get_name() const { return this->name_; }

  void publish(const SensorFrame &frame, float rssi) {
    if (frame.state != HandleState::UNCHANGED) {
      // Home Assistant lock device class: ON means unlocked.
      if (this->lock_ != nullptr)
        this->lock_->publish_state(frame.state == HandleState::UNLOCKED);
      if (this->tilt_ != nullptr)
        this->tilt_->publish_state(is_tilted(frame));
      if (this->state_ != nullptr)
        this->state_->publish_state(handle_state_to_string(frame.state));
    }
    if (this->rssi_ != nullptr)
      this->rssi_->publish_state(rssi);
  }

 protected:
  uint32_t device_id_;
  const char *name_;
  binary_sensor::BinarySensor *lock_{nullptr};
  binary_sensor::BinarySensor *tilt_{nullptr};
  text_sensor::TextSensor *state_{nullptr};
  sensor::Sensor *rssi_{nullptr};
};

class X3DRFM69 : public Component,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_8MHZ> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_dio0_pin(InternalGPIOPin *pin) { this->dio0_pin_ = pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_log_unclaimed_devices(bool log) { this->log_unclaimed_devices_ = log; }
  void set_log_raw_frames(bool log) { this->log_raw_frames_ = log; }

  void add_opening(X3DOpening *window) { this->openings_.push_back(window); }
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

  static void gpio_intr(X3DRFM69 *arg);

  InternalGPIOPin *dio0_pin_{nullptr};
  uint32_t frequency_{868950000};
  bool log_unclaimed_devices_{true};
  bool log_raw_frames_{false};
  uint8_t mode_{0xff};

  Deduplicator dedup_;
  std::vector<X3DOpening *> openings_;
  text_sensor::TextSensor *last_frame_{nullptr};
  Trigger<std::string, std::string, bool, std::string, float, std::string> event_trigger_;
};

}  // namespace esphome::x3d_rfm69
