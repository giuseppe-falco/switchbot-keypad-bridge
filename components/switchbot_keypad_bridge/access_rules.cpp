#include "access_rules.h"

#include <cstdlib>

namespace esphome {
namespace switchbot_keypad_bridge {

namespace {

std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  while (true) {
    const size_t pos = s.find(sep, start);
    out.push_back(s.substr(start, pos - start));
    if (pos == std::string::npos) return out;
    start = pos + 1;
  }
}

bool all_digits(const std::string &s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}

bool parse_method(char c, UnlockMethod &out) {
  switch (c) {
    case 'f': out = UnlockMethod::FINGERPRINT; return true;
    case 'p': out = UnlockMethod::PIN; return true;
    case 'n': out = UnlockMethod::NFC; return true;
    case 'c': out = UnlockMethod::FACE; return true;
    case 'a': out = UnlockMethod::PALM; return true;
    default: return false;
  }
}

// "HHMM" -> minutes; `allow_2400` admits the end-of-day value 1440.
bool parse_hhmm(const std::string &s, bool allow_2400, uint16_t &out) {
  if (s.size() != 4 || !all_digits(s)) return false;
  const int h = (s[0] - '0') * 10 + (s[1] - '0');
  const int m = (s[2] - '0') * 10 + (s[3] - '0');
  if (m > 59) return false;
  if (h == 24 && m == 0 && allow_2400) {
    out = 1440;
    return true;
  }
  if (h > 23) return false;
  out = static_cast<uint16_t>(h * 60 + m);
  return true;
}

bool parse_window(const std::string &s, AccessRules::Window &w) {
  const size_t slash = s.find('/');
  const size_t dash = s.find('-');
  if (slash == std::string::npos || dash == std::string::npos || dash < slash) return false;
  const std::string mask = s.substr(0, slash);
  if (mask.empty() || mask.size() > 2) return false;
  char *end = nullptr;
  const long days = std::strtol(mask.c_str(), &end, 16);
  if (*end != '\0' || days <= 0 || days > 0x7F) return false;
  if (!parse_hhmm(s.substr(slash + 1, dash - slash - 1), false, w.start)) return false;
  if (!parse_hhmm(s.substr(dash + 1), true, w.end)) return false;
  if (w.start == w.end) return false;  // ambiguous: use 0000-2400 for a full day
  w.days = static_cast<uint8_t>(days);
  return true;
}

}  // namespace

bool AccessRules::parse(const std::string &text, std::string &error) {
  if (text.size() >= ACCESS_RULES_MAX) {
    error = "rules too long";
    return false;
  }
  std::vector<Group> groups;
  if (!text.empty()) {
    for (const std::string &g : split(text, ';')) {
      const size_t eq = g.find('=');
      if (eq == std::string::npos) {
        error = "missing '=' in group '" + g + "'";
        return false;
      }
      Group group;
      for (const std::string &c : split(g.substr(0, eq), ',')) {
        UnlockMethod method;
        if (c.size() < 2 || !parse_method(c[0], method) || !all_digits(c.substr(1)) ||
            c.size() > 4) {
          error = "bad credential '" + c + "'";
          return false;
        }
        group.creds.emplace_back(method, std::atoi(c.c_str() + 1));
      }
      std::string rest = g.substr(eq + 1);
      const size_t at = rest.find('@');
      if (at != std::string::npos) {
        const std::string until = rest.substr(at + 1);
        if (!all_digits(until) || until.size() > 12) {
          error = "bad exception timestamp '" + until + "'";
          return false;
        }
        group.until = std::atoll(until.c_str());
        rest = rest.substr(0, at);
      }
      if (!rest.empty()) {
        for (const std::string &ws : split(rest, ',')) {
          Window w;
          if (!parse_window(ws, w)) {
            error = "bad window '" + ws + "'";
            return false;
          }
          group.windows.push_back(w);
        }
      }
      groups.push_back(std::move(group));
    }
  }
  this->groups_ = std::move(groups);
  this->text_ = text;
  return true;
}

bool AccessRules::window_admits_(const Window &w, const Clock &clock) {
  const uint8_t today = static_cast<uint8_t>(1u << clock.weekday);
  const uint8_t yesterday = static_cast<uint8_t>(1u << ((clock.weekday + 6) % 7));
  if (w.start < w.end) {
    return (w.days & today) && clock.minute >= w.start && clock.minute < w.end;
  }
  // Crosses midnight: the late part belongs to today, the early part to the
  // window that started yesterday.
  return ((w.days & today) && clock.minute >= w.start) ||
         ((w.days & yesterday) && clock.minute < w.end);
}

AccessRules::Decision AccessRules::check(bool enabled, UnlockMethod method, int index,
                                         const Clock &clock) const {
  if (!enabled) return Decision::ALLOWED_DISABLED;
  const Group *match = nullptr;
  for (const Group &g : this->groups_) {
    for (const auto &c : g.creds) {
      if (c.first == method && c.second == index) {
        match = &g;
        break;
      }
    }
    if (match != nullptr) break;
  }
  if (match == nullptr) return Decision::ALLOWED_NO_RULE;
  if (!clock.valid) return Decision::ALLOWED_NO_CLOCK;
  if (match->until > 0 && clock.timestamp < match->until) return Decision::ALLOWED_EXCEPTION;
  for (const Window &w : match->windows) {
    if (window_admits_(w, clock)) return Decision::ALLOWED_WINDOW;
  }
  return Decision::DENIED_SCHEDULE;
}

const char *AccessRules::decision_name(Decision d) {
  switch (d) {
    case Decision::ALLOWED_DISABLED: return "disabled";
    case Decision::ALLOWED_NO_RULE: return "no_rule";
    case Decision::ALLOWED_NO_CLOCK: return "no_clock";
    case Decision::ALLOWED_WINDOW: return "window";
    case Decision::ALLOWED_EXCEPTION: return "exception";
    case Decision::DENIED_SCHEDULE: return "outside_schedule";
  }
  return "unknown";
}

}  // namespace switchbot_keypad_bridge
}  // namespace esphome
