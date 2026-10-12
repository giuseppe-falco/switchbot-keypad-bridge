#pragma once

#include "esphome/core/automation.h"
#include "switchbot_keypad_bridge.h"

namespace esphome {
namespace switchbot_keypad_bridge {

template<typename... Ts> class RearmAction : public Action<Ts...> {
 public:
  explicit RearmAction(SwitchbotKeypadBridge *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->rearm(); }

 protected:
  SwitchbotKeypadBridge *parent_;
};

#ifdef USE_SKB_ACCESS_CONTROL
template<typename... Ts> class SetAccessRulesAction : public Action<Ts...> {
 public:
  explicit SetAccessRulesAction(SwitchbotKeypadBridge *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, rules)
  void play(const Ts &...x) override { this->parent_->set_access_rules(this->rules_.value(x...)); }

 protected:
  SwitchbotKeypadBridge *parent_;
};
#endif

class LockTrigger : public Trigger<> {
 public:
  explicit LockTrigger(SwitchbotKeypadBridge *parent) {
    parent->add_on_lock_callback([this]() { this->trigger(); });
  }
};

class UnlockTrigger : public Trigger<std::string, int> {
 public:
  explicit UnlockTrigger(SwitchbotKeypadBridge *parent) {
    parent->add_on_unlock_callback(
        [this](const std::string &method, int index) { this->trigger(method, index); });
  }
};

class DoorbellTrigger : public Trigger<> {
 public:
  explicit DoorbellTrigger(SwitchbotKeypadBridge *parent) {
    parent->add_on_doorbell_callback([this]() { this->trigger(); });
  }
};

class DeniedTrigger : public Trigger<std::string, int, std::string> {
 public:
  explicit DeniedTrigger(SwitchbotKeypadBridge *parent) {
    parent->add_on_denied_callback(
        [this](const std::string &method, int index, const std::string &reason) {
          this->trigger(method, index, reason);
        });
  }
};

}  // namespace switchbot_keypad_bridge
}  // namespace esphome
