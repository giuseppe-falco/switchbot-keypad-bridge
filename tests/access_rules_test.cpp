#include <cassert>
#include <cstdio>
#include <string>

#include "components/switchbot_keypad_bridge/access_rules.h"

using esphome::switchbot_keypad_bridge::AccessRules;
using esphome::switchbot_keypad_bridge::UnlockMethod;
using D = AccessRules::Decision;

namespace {

constexpr uint8_t MON = 0, FRI = 4, SAT = 5, SUN = 6;

AccessRules::Clock at(uint8_t weekday, int hh, int mm, int64_t ts = 1000) {
  AccessRules::Clock c;
  c.valid = true;
  c.weekday = weekday;
  c.minute = static_cast<uint16_t>(hh * 60 + mm);
  c.timestamp = ts;
  return c;
}

D fp(const AccessRules &r, int idx, const AccessRules::Clock &c) {
  return r.check(true, UnlockMethod::FINGERPRINT, idx, c);
}

void expect_reject(const char *text) {
  AccessRules r;
  std::string err;
  assert(r.parse("f1=7F/0000-2400", err));
  if (r.parse(text, err)) {
    std::fprintf(stderr, "accepted invalid rules: %s\n", text);
    assert(false);
  }
  assert(!err.empty());
  // A rejected update keeps the previous rules.
  assert(r.text() == "f1=7F/0000-2400");
}

}  // namespace

int main() {
  std::string err;

  // Empty rules: nobody restricted.
  AccessRules none;
  assert(none.parse("", err));
  assert(none.group_count() == 0);
  assert(fp(none, 3, at(MON, 3, 0)) == D::ALLOWED_NO_RULE);

  // Weekdays 07:00-23:00, weekend 09:00-02:00 (crosses midnight).
  AccessRules r;
  assert(r.parse("f3,f5,p2=1F/0700-2300,60/0900-0200", err));
  assert(r.group_count() == 1);

  // Switched off: everything allowed.
  assert(r.check(false, UnlockMethod::FINGERPRINT, 3, at(MON, 3, 0)) == D::ALLOWED_DISABLED);
  // Unlisted credential and other methods with the same index are unrestricted.
  assert(fp(r, 4, at(MON, 3, 0)) == D::ALLOWED_NO_RULE);
  assert(r.check(true, UnlockMethod::PIN, 3, at(MON, 3, 0)) == D::ALLOWED_NO_RULE);

  // Weekday window, start inclusive and end exclusive.
  assert(fp(r, 3, at(MON, 6, 59)) == D::DENIED_SCHEDULE);
  assert(fp(r, 3, at(MON, 7, 0)) == D::ALLOWED_WINDOW);
  assert(fp(r, 5, at(FRI, 22, 59)) == D::ALLOWED_WINDOW);
  assert(fp(r, 5, at(FRI, 23, 0)) == D::DENIED_SCHEDULE);
  assert(r.check(true, UnlockMethod::PIN, 2, at(MON, 12, 0)) == D::ALLOWED_WINDOW);

  // Weekend window crossing midnight.
  assert(fp(r, 3, at(SAT, 8, 59)) == D::DENIED_SCHEDULE);
  assert(fp(r, 3, at(SAT, 23, 30)) == D::ALLOWED_WINDOW);
  assert(fp(r, 3, at(SUN, 1, 59)) == D::ALLOWED_WINDOW);   // Saturday's window
  assert(fp(r, 3, at(SUN, 2, 0)) == D::DENIED_SCHEDULE);
  assert(fp(r, 3, at(MON, 1, 0)) == D::ALLOWED_WINDOW);    // Sunday's window spills into Monday
  assert(fp(r, 3, at(MON, 2, 0)) == D::DENIED_SCHEDULE);
  // Friday night does not inherit the weekend spill-over: no Thursday-late window.
  assert(fp(r, 3, at(FRI, 1, 0)) == D::DENIED_SCHEDULE);

  // No valid clock: fail open, but only for restricted credentials.
  AccessRules::Clock no_clock;
  assert(fp(r, 3, no_clock) == D::ALLOWED_NO_CLOCK);

  // Full day with 2400, and an exception-only group with no windows.
  AccessRules full;
  assert(full.parse("f1=7F/0000-2400;p0=@5000", err));
  assert(fp(full, 1, at(SUN, 23, 59)) == D::ALLOWED_WINDOW);
  assert(full.check(true, UnlockMethod::PIN, 0, at(MON, 12, 0, 4999)) == D::ALLOWED_EXCEPTION);
  assert(full.check(true, UnlockMethod::PIN, 0, at(MON, 12, 0, 5000)) == D::DENIED_SCHEDULE);

  // Exception overrides the schedule until it expires.
  AccessRules exc;
  assert(exc.parse("f3=1F/0700-2300@2000", err));
  assert(fp(exc, 3, at(SAT, 3, 0, 1999)) == D::ALLOWED_EXCEPTION);
  assert(fp(exc, 3, at(SAT, 3, 0, 2000)) == D::DENIED_SCHEDULE);

  // Several groups: first match wins, other groups untouched.
  AccessRules multi;
  assert(multi.parse("f3=1F/0700-2300;f8,n1=7F/0600-2200", err));
  assert(multi.group_count() == 2);
  assert(fp(multi, 8, at(SUN, 21, 0)) == D::ALLOWED_WINDOW);
  assert(multi.check(true, UnlockMethod::NFC, 1, at(SUN, 23, 0)) == D::DENIED_SCHEDULE);

  // Invalid input is rejected and keeps the previous rules.
  expect_reject("f3");                     // no '='
  expect_reject("x3=7F/0700-2300");        // unknown method
  expect_reject("f=7F/0700-2300");         // missing index
  expect_reject("f12345=7F/0700-2300");    // index too long
  expect_reject("f3=80/0700-2300");        // day mask out of range
  expect_reject("f3=0/0700-2300");         // empty day mask
  expect_reject("f3=7F/0760-2300");        // minutes > 59
  expect_reject("f3=7F/2400-0100");        // 2400 only valid as end
  expect_reject("f3=7F/0700-0700");        // zero-length window
  expect_reject("f3=7F/700-2300");         // short time
  expect_reject("f3=7F/0700-2300@12ab");   // bad timestamp
  expect_reject("f3=7F/0700-2300;");       // empty trailing group
  expect_reject(std::string(1100, 'f').c_str());  // too long

  std::puts("access_rules: all tests passed");
  return 0;
}
