#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/x3d_rfm69/x3d_rfm69.h"

namespace esphome::x3d_rfm69 {

class X3DWindowBinarySensor : public X3DListener {
 public:
  using X3DListener::X3DListener;

  void set_lock_sensor(binary_sensor::BinarySensor *sensor) { this->lock_ = sensor; }

  void on_window_event(const WindowEvent &event) override {
    const WindowState state = event.frame.state;
    // Home Assistant lock device class: ON means unlocked.
    if (this->lock_ != nullptr && state != WindowState::UNKNOWN)
      this->lock_->publish_state(state == WindowState::UNLOCKED);
  }

 protected:
  binary_sensor::BinarySensor *lock_{nullptr};
};

}  // namespace esphome::x3d_rfm69
