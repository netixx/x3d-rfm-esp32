#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/x3d_rfm69/x3d_rfm69.h"

namespace esphome::x3d_rfm69 {

class X3DWindowSensor : public X3DListener {
 public:
  using X3DListener::X3DListener;

  void set_rssi_sensor(sensor::Sensor *sensor) { this->rssi_ = sensor; }

  void on_window_event(const WindowEvent &event) override {
    if (this->rssi_ != nullptr)
      this->rssi_->publish_state(event.rssi);
  }

 protected:
  sensor::Sensor *rssi_{nullptr};
};

}  // namespace esphome::x3d_rfm69
