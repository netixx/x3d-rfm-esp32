#pragma once

#include "esphome/components/lock/lock.h"
#include "esphome/components/x3d_rfm69/x3d_rfm69.h"
#include "esphome/core/log.h"

namespace esphome::x3d_rfm69 {

/// Read-only lock entity following the handle of one X3D device.
///
/// Workaround: Home Assistant's area dashboard only lists `lock` entities in
/// its Security section, not `binary_sensor` with device class lock. The radio
/// is receive-only, so lock / unlock / open commands are refused and the real
/// state is published again.
class X3DLock : public lock::Lock, public HandleListener {
 public:
  void on_handle(HandleState state, bool /*tilted*/) override {
    this->publish_state(state == HandleState::LOCKED ? lock::LOCK_STATE_LOCKED : lock::LOCK_STATE_UNLOCKED);
  }

 protected:
  void control(const lock::LockCall &/*call*/) override {
    ESP_LOGW("x3d_rfm69.lock", "'%s' is read-only: the handle can't be moved remotely", this->get_name().c_str());
    this->publish_state(this->state);
  }
  void open_latch() override { this->control(this->make_call()); }
};

}  // namespace esphome::x3d_rfm69
