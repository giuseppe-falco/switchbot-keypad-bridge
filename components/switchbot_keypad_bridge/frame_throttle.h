#pragma once

#include <cstdint>

namespace esphome {
namespace switchbot_keypad_bridge {

// Rate limit for keypad frames the session rejects. A legitimate keypad almost
// never sends one; an attacker brute-forcing ciphertext under an IV it
// requested (a 4-byte state poll is recognised by 3 plaintext bytes, and its
// known reply leaks keystream) sends many. After `limit` rejected frames
// within one window, every keypad frame is ignored for `lockout_ms`.
// All methods run on the ESPHome main task; times are millis() and
// wrap-safe. A limit of 0 disables the throttle.
class FrameThrottle {
 public:
  void configure(uint32_t limit, uint32_t window_ms, uint32_t lockout_ms) {
    this->limit_ = limit;
    this->window_ms_ = window_ms;
    this->lockout_ms_ = lockout_ms;
  }
  bool enabled() const { return this->limit_ != 0; }

  // True while a lockout is running; clears it once it has expired.
  bool blocked(uint32_t now) {
    if (!this->locked_) return false;
    if (static_cast<uint32_t>(now - this->locked_at_) < this->lockout_ms_) return true;
    this->locked_ = false;
    this->count_ = 0;
    return false;
  }

  // Records one rejected frame. Returns true exactly when this frame starts
  // a lockout.
  bool record_invalid(uint32_t now) {
    if (this->limit_ == 0 || this->locked_) return false;
    if (this->count_ == 0 || static_cast<uint32_t>(now - this->window_start_) >= this->window_ms_) {
      this->window_start_ = now;
      this->count_ = 0;
    }
    if (++this->count_ < this->limit_) return false;
    this->locked_ = true;
    this->locked_at_ = now;
    this->count_ = 0;
    return true;
  }

 protected:
  uint32_t limit_{0};
  uint32_t window_ms_{0};
  uint32_t lockout_ms_{0};
  uint32_t count_{0};
  uint32_t window_start_{0};
  uint32_t locked_at_{0};
  bool locked_{false};
};

}  // namespace switchbot_keypad_bridge
}  // namespace esphome
