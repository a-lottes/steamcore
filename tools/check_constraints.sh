#!/usr/bin/env bash
# Greps the delivered engine tree for violations of constitution §3/§4/§6
# that would otherwise rely on reviewer diligence: dynamic allocation,
# ESP-IDF/FreeRTOS/driver headers in logic code, and resolution literals
# outside the single compile-time constant. Run via `make lint`.
set -euo pipefail

# Resolve to the repo root regardless of the caller's working directory —
# `make lint` always runs from the root, but a human running this script
# directly (e.g. from tools/) must get the same result, not a silent
# no-op. Every `grep` below runs as the condition of an `if`, where `set
# -e` never fires on a non-zero exit; a missing/wrong INCLUDE_DIR/SRC_DIR
# used to mean `grep` failed with "No such file or directory", the `if`
# read that as "no match", and the script printed `make lint OK` having
# checked nothing (review F15).
cd "$(dirname "${BASH_SOURCE[0]}")/.."

INCLUDE_DIR="firmware/steamcore/include"
SRC_DIR="firmware/steamcore/src"

for d in "$INCLUDE_DIR" "$SRC_DIR"; do
  if [ ! -d "$d" ]; then
    echo "check_constraints: expected directory '$d' (relative to repo root) does not exist -- refusing to report a false OK" >&2
    exit 1
  fi
done

fail=0

report() {
  echo "check_constraints: $1"
  fail=1
}

echo "--- no dynamic allocation or heap-backed container (constitution §3/§6) ---"
if grep -rnE '\bnew\b|\b(m|c|re)alloc[[:space:]]*\(|\bstrdup[[:space:]]*\(|std::(vector|string|map|deque|list|function|unique_ptr|make_unique|shared_ptr|make_shared)\b' \
    "$INCLUDE_DIR" "$SRC_DIR"; then
  report "dynamic allocation or a forbidden container/string/smart-pointer type was found above"
fi

echo "--- no ESP-IDF/FreeRTOS/driver header in logic code (constitution §4) ---"
if grep -rnE '#include[[:space:]]*[<"](esp_[A-Za-z0-9_]*\.h|esp32s3/|freertos/|driver/|hal/|soc/|sdkconfig\.h|nvs_flash\.h)' \
    "$INCLUDE_DIR" "$SRC_DIR"; then
  report "an ESP-IDF/FreeRTOS/driver header was found above -- logic code must stay hardware-free"
fi

# Excludes lines that are themselves a `//` comment (this codebase uses
# no other comment style — no multi-line /* */ block ever appears as a
# standalone line here) so prose like "a 240x160 framebuffer" or a
# doc-comment usage example ("fillRect(10, 10, 32, 16, ...)") is not a
# false positive -- lint checks code, not prose. Matching only a leading
# `//` (not a bare `*`) matters: a bare-`*` exclusion would also hide a
# real pointer-dereference statement like "*p = 16;" from the check
# (review F13).

echo "--- no resolution literal outside config.h (constitution §3) ---"
if grep -rnE --include='*.h' --include='*.cpp' '\b(240|160|480|320)\b' \
    "$INCLUDE_DIR" "$SRC_DIR" \
    | grep -v '/config\.h:' \
    | grep -vE ':[0-9]+:[[:space:]]*//'; then
  report "a resolution literal (240/160/480/320) was found outside config.h"
fi

echo "--- no tile-size literal outside config.h (constitution §3, T4) ---"
if grep -rnE --include='*.h' --include='*.cpp' '\b16\b' \
    "$INCLUDE_DIR" "$SRC_DIR" \
    | grep -v '/config\.h:' \
    | grep -vE ':[0-9]+:[[:space:]]*//'; then
  report "a tile-size literal (16) was found outside config.h"
fi

if [ "$fail" -ne 0 ]; then
  echo "make lint FAILED"
  exit 1
fi
echo "make lint OK"
