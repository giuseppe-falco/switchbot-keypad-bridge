#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN=$(mktemp "${TMPDIR:-/tmp}/switchbot-access-rules-test.XXXXXX")
trap 'rm -f "$BIN"' EXIT HUP INT TERM
CXX=${CXX:-c++}

"$CXX" -std=c++17 -Wall -Wextra -Werror -I"$ROOT/tests/stubs" -I"$ROOT" \
  "$ROOT/components/switchbot_keypad_bridge/access_rules.cpp" \
  "$ROOT/components/switchbot_keypad_bridge/lock_protocol.cpp" \
  "$ROOT/tests/access_rules_test.cpp" -o "$BIN"
"$BIN"
