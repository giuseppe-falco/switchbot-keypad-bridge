#include <cassert>
#include <cstdint>
#include <cstdio>

#include "components/switchbot_keypad_bridge/frame_throttle.h"

using esphome::switchbot_keypad_bridge::FrameThrottle;

int main() {
  // Disabled by default: never locks.
  FrameThrottle off;
  for (uint32_t i = 0; i < 1000; ++i) assert(!off.record_invalid(i));
  assert(!off.blocked(1000));

  // 3 rejected frames within 10 s lock the keypad out for 10 s.
  FrameThrottle t;
  t.configure(3, 10000, 10000);
  assert(!t.record_invalid(0));
  assert(!t.record_invalid(100));
  assert(!t.blocked(150));
  assert(t.record_invalid(200));        // third one starts the lockout
  assert(t.blocked(200));
  assert(!t.record_invalid(300));       // ignored while locked
  assert(t.blocked(10199));
  assert(!t.blocked(10200));            // lockout over, counter cleared
  assert(!t.record_invalid(10300));
  assert(!t.record_invalid(10400));
  assert(t.record_invalid(10500));

  // Slow trickle never trips: the window restarts.
  FrameThrottle slow;
  slow.configure(3, 10000, 10000);
  for (uint32_t i = 0; i < 20; ++i) {
    assert(!slow.record_invalid(i * 6000));  // at most 2 per window
  }

  // millis() wrap-around inside a window and a lockout.
  FrameThrottle wrap;
  wrap.configure(2, 10000, 10000);
  assert(!wrap.record_invalid(UINT32_MAX - 100));
  assert(wrap.record_invalid(50));
  assert(wrap.blocked(9000));
  assert(wrap.blocked(10049));
  assert(!wrap.blocked(10050));

  std::puts("frame_throttle: all tests passed");
  return 0;
}
