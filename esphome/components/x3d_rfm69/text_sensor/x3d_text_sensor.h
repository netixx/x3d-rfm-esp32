#pragma once

#include <cinttypes>
#include <cstdio>

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/x3d_rfm69/x3d_rfm69.h"

namespace esphome::x3d_rfm69 {

class X3DWindowTextSensor : public X3DListener {
 public:
  using X3DListener::X3DListener;

  void set_state_sensor(text_sensor::TextSensor *sensor) { this->state_ = sensor; }
  void set_last_frame_sensor(text_sensor::TextSensor *sensor) { this->last_frame_ = sensor; }

  void on_window_event(const WindowEvent &event) override {
    const auto &frame = event.frame;
    if (this->state_ != nullptr && frame.state != WindowState::UNKNOWN)
      this->state_->publish_state(window_state_to_string(frame.state));
    if (this->last_frame_ != nullptr) {
      char buf[64];
      snprintf(buf, sizeof(buf), "%06" PRIX32 " %s (msg 0x%02X, %.1f dBm)", frame.device_id,
               window_state_to_string(frame.state), frame.msg_no, event.rssi);
      this->last_frame_->publish_state(buf);
    }
  }

 protected:
  text_sensor::TextSensor *state_{nullptr};
  text_sensor::TextSensor *last_frame_{nullptr};
};

}  // namespace esphome::x3d_rfm69
