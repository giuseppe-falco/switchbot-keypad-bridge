#pragma once

// Optional per-credential access schedule, evaluated on the ESP itself so it
// keeps working while Home Assistant is down. Home Assistant (or any API
// client) pushes a compact rule string; the bridge persists it to NVS and
// checks every keypad unlock against it before relaying to the physical lock.
//
// Rule string grammar (no spaces):
//
//   rules   := "" | group (";" group)*
//   group   := creds "=" windows ["@" until]
//   creds   := cred ("," cred)*
//   cred    := method index        method: f=fingerprint p=pin n=nfc c=face a=palm
//   windows := "" | window ("," window)*
//   window  := daymask "/" HHMM "-" HHMM
//   daymask := 1-2 hex digits, bit0 = Monday ... bit6 = Sunday
//   until   := Unix timestamp (seconds); unlocks are allowed until then
//
// A window whose end is not after its start crosses midnight and belongs to
// the day it starts on ("sat 22:00-02:00" also admits Sunday 01:30). "2400"
// is a valid end. Example — fingerprints 3/5 and PIN 2, weekdays 07-23 plus
// weekend 09-02, with a temporary exception until a timestamp:
//
//   f3,f5,p2=1F/0700-2300,60/0900-0200@1791936000
//
// Credentials not listed in any group are always allowed: the schedule only
// restricts the people it names. Pure logic, no ESPHome dependency, so the
// host tests in tests/ can exercise it.

#include <cstdint>
#include <string>
#include <vector>

#include "lock_protocol.h"

namespace esphome {
namespace switchbot_keypad_bridge {

// Upper bound (including the trailing NUL) on the persisted rule string.
constexpr size_t ACCESS_RULES_MAX = 1024;

class AccessRules {
 public:
  enum class Decision : uint8_t {
    ALLOWED_DISABLED,   // access control switched off
    ALLOWED_NO_RULE,    // credential not restricted
    ALLOWED_NO_CLOCK,   // restricted, but no valid time: fail open
    ALLOWED_WINDOW,     // inside one of its windows
    ALLOWED_EXCEPTION,  // temporary exception still running
    DENIED_SCHEDULE,    // restricted and outside every window
  };

  struct Window {
    uint8_t days{0};      // bit0 = Monday ... bit6 = Sunday
    uint16_t start{0};    // minutes from midnight, 0..1439
    uint16_t end{0};      // minutes from midnight, 1..1440
  };

  struct Group {
    std::vector<std::pair<UnlockMethod, int>> creds;
    std::vector<Window> windows;
    int64_t until{0};     // 0 = no exception
  };

  // Local wall-clock snapshot handed in by the caller.
  struct Clock {
    bool valid{false};
    uint8_t weekday{0};   // 0 = Monday ... 6 = Sunday
    uint16_t minute{0};   // minutes from midnight
    int64_t timestamp{0}; // Unix seconds
  };

  // Parses `text`. On failure returns false, fills `error` and leaves the
  // current rules untouched.
  bool parse(const std::string &text, std::string &error);

  Decision check(bool enabled, UnlockMethod method, int index, const Clock &clock) const;

  const std::string &text() const { return this->text_; }
  size_t group_count() const { return this->groups_.size(); }

  static bool is_allowed(Decision d) { return d != Decision::DENIED_SCHEDULE; }
  static const char *decision_name(Decision d);

 protected:
  static bool window_admits_(const Window &w, const Clock &clock);

  std::string text_;
  std::vector<Group> groups_;
};

}  // namespace switchbot_keypad_bridge
}  // namespace esphome
