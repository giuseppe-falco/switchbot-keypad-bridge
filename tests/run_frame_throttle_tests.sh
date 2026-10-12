#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN=$(mktemp "${TMPDIR:-/tmp}/switchbot-frame-throttle-test.XXXXXX")
trap 'rm -f "$BIN"' EXIT HUP INT TERM
CXX=${CXX:-c++}

"$CXX" -std=c++17 -Wall -Wextra -Werror -I"$ROOT" \
  "$ROOT/tests/frame_throttle_test.cpp" -o "$BIN"
"$BIN"
