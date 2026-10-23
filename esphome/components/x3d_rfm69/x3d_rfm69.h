#pragma once

#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"

#include "x3d_frame.h"

namespace esphome::x3d_rfm69 {

/// Decoded event, handed to listeners once per new (device, msg#).
struct WindowEvent {
  const SensorFrame &frame;
  float rssi;
};

/// Receives events of one device, or of every device when ANY_DEVICE is used.
class X3DListener {
 public:
  static constexpr uint32_t ANY_DEVICE = 0xFFFFFFFF;

  explicit X3DListener(uint32_t device_id) : device_id_(device_id) {}
  uint32_t get_device_id() const { return this->device_id_; }
  bool is_any_device() const { return this->device_id_ == ANY_DEVICE; }
  virtual void on_window_event(const WindowEvent &event) = 0;

 protected:
  uint32_t device_id_;
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
  void set_log_unknown_devices(bool log) { this->log_unknown_devices_ = log; }
  void set_log_raw_frames(bool log) { this->log_raw_frames_ = log; }

  void register_listener(X3DListener *listener);
  /// (device_id hex string, state, kind, rssi, raw frame hex), once per new event of any device.
  Trigger<std::string, std::string, std::string, float, std::string> *get_window_event_trigger() {
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
  bool log_unknown_devices_{true};
  bool log_raw_frames_{false};
  uint8_t mode_{0xff};

  Deduplicator dedup_;
  std::vector<X3DListener *> listeners_;
  Trigger<std::string, std::string, std::string, float, std::string> event_trigger_;
};

}  // namespace esphome::x3d_rfm69
