#pragma once

#include "esphome/components/lock/lock.h"
#include "esphome/components/x3d_rfm69/x3d_rfm69.h"
#include "esphome/core/log.h"

namespace esphome::x3d_rfm69 {

/// Read-only lock entity following the handle of one X3D device, or the
/// combined state of several sashes (`x3d_id: [left, right]`).
///
/// Workaround: Home Assistant's area dashboard only lists `lock` entities in
/// its Security section, not `binary_sensor` with device class lock. The radio
/// is receive-only, so lock / unlock / open commands are refused and the real
/// state is published again.
class X3DLock : public lock::Lock, public HandleListener {
 public:
  void add_x3d_id(uint32_t x3d_id) { this->combiner_.add(x3d_id); }
  void on_handle(uint32_t x3d_id, HandleState state, bool /*tilted*/) override {
    this->publish_(this->combiner_.update(x3d_id, state));
  }
  void on_timeout(uint32_t x3d_id) override { this->publish_(this->combiner_.update(x3d_id, HandleState::UNCHANGED)); }

 protected:
  void control(const lock::LockCall &/*call*/) override {
    ESP_LOGW("x3d_rfm69.lock", "'%s' is read-only: the handle can't be moved remotely", this->get_name().c_str());
    this->publish_state(this->state);
  }
  void open_latch() override { this->control(this->make_call()); }

  void publish_(HandleState combined) {
    if (combined == HandleState::LOCKED) {
      this->publish_state(lock::LOCK_STATE_LOCKED);
    } else if (combined == HandleState::UNLOCKED) {
      this->publish_state(lock::LOCK_STATE_UNLOCKED);
    } else {
      this->publish_state(lock::LOCK_STATE_NONE);
    }
  }

  HandleCombiner combiner_;
};

}  // namespace esphome::x3d_rfm69
