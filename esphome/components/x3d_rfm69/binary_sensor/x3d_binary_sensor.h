#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/x3d_rfm69/x3d_rfm69.h"

namespace esphome::x3d_rfm69 {

class X3DWindowBinarySensor : public X3DListener {
 public:
  using X3DListener::X3DListener;

  void set_opening_sensor(binary_sensor::BinarySensor *sensor) { this->opening_ = sensor; }
  void set_lock_sensor(binary_sensor::BinarySensor *sensor) { this->lock_ = sensor; }

  void on_window_event(const WindowEvent &event) override {
    const WindowState state = event.frame.state;
    if (state == WindowState::UNKNOWN)
      return;
    if (this->opening_ != nullptr)
      this->opening_->publish_state(state == WindowState::OPEN);
    // Home Assistant lock device class: ON means unlocked. An open window can't be locked.
    if (this->lock_ != nullptr)
      this->lock_->publish_state(state != WindowState::CLOSED_LOCKED);
  }

 protected:
  binary_sensor::BinarySensor *opening_{nullptr};
  binary_sensor::BinarySensor *lock_{nullptr};
};

}  // namespace esphome::x3d_rfm69
