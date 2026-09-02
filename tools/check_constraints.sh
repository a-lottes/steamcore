#!/usr/bin/env bash
# Greps the delivered engine tree -- include/, src/, and (for the
# text-rendering and game-loop rules below, which need to reach specific
# test/fixture files no include/src scan touches) test/ -- for violations
# of constitution §3/§4/§6 that would otherwise rely on reviewer
# diligence: dynamic allocation, ESP-IDF/FreeRTOS/driver headers in logic
# code, resolution/tile/glyph-metric literals outside their one
# compile-time constant, and (game-loop) a wall-clock read or unseeded
# RNG call where the determinism guarantee depends on there being none.
# Run via `make lint`.
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
TEST_DIR="firmware/steamcore/test"

for d in "$INCLUDE_DIR" "$SRC_DIR" "$TEST_DIR"; do
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
# `//`-line exclusion (same reasoning as the literal checks below): "new"
# is an ordinary English word ("a new low-level drawing primitive") long
# before it is ever a C++ keyword, and a doc comment must be free to say
# it. Code that actually calls `new` is never itself a `//` comment line.
if grep -rnE '\bnew\b|\b(m|c|re)alloc[[:space:]]*\(|\bstrdup[[:space:]]*\(|std::(vector|string|map|deque|list|function|unique_ptr|make_unique|shared_ptr|make_shared)\b' \
    "$INCLUDE_DIR" "$SRC_DIR" \
    | grep -vE ':[0-9]+:[[:space:]]*//'; then
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

echo "--- no glyph-metric literal (8 or 43) outside font.h (text-rendering, NFR-4) ---"
# Scope: font.cpp and text.{h,cpp} always; plus any other include/src
# file that #includes either header (currently none). font.h itself is
# excluded -- it is where kGlyphWidth/kGlyphHeight/kGlyphAdvance/
# kGlyphCount are DEFINED, so the literals 8 and 43 belong there and
# nowhere else, the same "one compile-time constant, everyone else reads
# it" rule already applied to the screen resolution and the tile size.
#
# Two exclusions are needed, not one: the usual `//`-comment lines, AND
# single-quoted character literals -- font.cpp's glyph table is keyed by
# `{'8', {...}}` and `{'4', {...}}`-style entries, and stripping only
# comments would make every digit-named glyph a false positive (review
# T10 self-check). Row strings never need this: the compile-time art
# validator (font.cpp) only ever lets them contain ' '/'#'.
TEXT_MODULE_FILES="$SRC_DIR/font.cpp $INCLUDE_DIR/steamcore/text.h $SRC_DIR/text.cpp"
for f in $(grep -rlE '#include[[:space:]]*"steamcore/(font|text)\.h"' \
    "$INCLUDE_DIR" "$SRC_DIR" 2>/dev/null); do
  case "$f" in
    */font.h|*/text.h) continue ;;  # the definitions themselves, not consumers
  esac
  case " $TEXT_MODULE_FILES " in
    *" $f "*) ;;
    *) TEXT_MODULE_FILES="$TEXT_MODULE_FILES $f" ;;
  esac
done

# The character-literal strip must remove only the literal itself, never
# the whole line: dropping the line let a real violation hide behind any
# character literal that happened to share it -- `char c = '8'; foo(8);`
# and `int32_t n = 43;  // the 'n' glyphs` both passed this gate clean
# (review F2). awk, not grep, because the match has to be made against
# the stripped text while the report still names the file and the
# original line -- a `grep -n` prefix would itself match `\b8\b`.
if awk -v q="'" '
  /^[[:space:]]*\/\// { next }
  {
    stripped = $0
    gsub(q "\\\\?." q, "", stripped)
    if (stripped ~ /(^|[^0-9A-Za-z_])(8|43)([^0-9A-Za-z_]|$)/) {
      printf "%s:%d:%s\n", FILENAME, FNR, $0
      found = 1
    }
  }
  END { exit(found ? 0 : 1) }
' $TEXT_MODULE_FILES; then
  report "a glyph-metric literal (8 or 43) was found outside font.h in the text-rendering module"
fi

echo "--- no wall-clock read or unseeded RNG in the game-loop mechanism (AC-1.3, AC-4.4) ---"
# Scope: the mechanism itself plus its test/fixture files -- deliberately
# NOT bench_game_loop.cpp, which legitimately measures elapsed time with
# <chrono> the same way bench_dirty_scan.cpp and bench_text.cpp already
# do. A tick is a caller-driven discrete step (constitution §3 Timing,
# spec A8): nothing that decides what a game sees may read a clock or an
# unseeded random source, because that is exactly what would break
# AC-4.1's byte-identical-replay guarantee.
GAME_LOOP_DETERMINISM_FILES="$INCLUDE_DIR/steamcore/game_loop.h $TEST_DIR/game_loop_test.cpp $TEST_DIR/game_loop_determinism_test.cpp $TEST_DIR/fb_compare.h $TEST_DIR/fb_compare_test.cpp $TEST_DIR/replay_fixture.h"
# A missing file in this explicitly-named set is a script bug, not a
# quiet no-op -- same posture as the directory guard above (review F6:
# `[ -f "$f" ] || continue` used to skip a renamed/deleted file silently,
# so `make lint OK` could report a mechanism that was never scanned).
for f in $GAME_LOOP_DETERMINISM_FILES; do
  if [ ! -f "$f" ]; then
    report "expected game-loop file '$f' does not exist -- refusing to skip it silently"
    continue
  fi
  if grep -nHE '<chrono>|<ctime>|<time\.h>|<sys/time\.h>|std::chrono|steady_clock|system_clock|high_resolution_clock|\bclock[[:space:]]*\(|\bclock_gettime[[:space:]]*\(|\btime[[:space:]]*\(|gettimeofday|\brand[[:space:]]*\(|\bsrand[[:space:]]*\(|random_device|<random>' \
      "$f" | grep -vE ':[0-9]+:[[:space:]]*//'; then
    report "a wall-clock read or unseeded RNG call was found above, in a file the determinism guarantee (AC-4.1) depends on"
  fi
done

echo "--- no dynamic allocation in the game-loop file set (NFR-2, extends the include/src grep to test/) ---"
# The include/src allocation check above never reaches test/, so this
# feature's mechanism-adjacent test and fixture files -- and its bench,
# which has no reason to allocate either -- get their own pass over the
# same pattern. Same missing-file posture as the loop above (review F6).
GAME_LOOP_ALLOC_FILES="$GAME_LOOP_DETERMINISM_FILES $TEST_DIR/bench_game_loop.cpp"
for f in $GAME_LOOP_ALLOC_FILES; do
  if [ ! -f "$f" ]; then
    report "expected game-loop file '$f' does not exist -- refusing to skip it silently"
    continue
  fi
  if grep -nHE '\bnew\b|\b(m|c|re)alloc[[:space:]]*\(|\bstrdup[[:space:]]*\(|std::(vector|string|map|deque|list|function|unique_ptr|make_unique|shared_ptr|make_shared)\b' \
      "$f" | grep -vE ':[0-9]+:[[:space:]]*//'; then
    report "dynamic allocation or a forbidden container/string/smart-pointer type was found above, in the game-loop file set"
  fi
done

echo "--- tools/*.py imports only from the standard library (constitution NFR-4) ---"
# Allowlist, not a denylist: an unrecognised import fails closed rather
# than trusting a list of known-bad packages we might not think of
# (spec A5/C8 -- no pip install, no Pillow). "fb_view" is this project's
# own local module, not a stdlib one, and is allowed for that reason.
PY_ALLOWED_IMPORTS="argparse os re struct subprocess sys tempfile time unittest zlib fb_view __future__"
# Matching must not be anchored to column 0 and must split a comma list:
# an indented `import requests` (inside a function or a
# `try:`/`except ImportError:` block -- the canonical way an optional
# Pillow dependency gets introduced) and `import os, requests` both used
# to pass this check clean (review F1). Known residual: a deliberately
# obfuscated `__import__("requests")` is still not detected -- this is a
# grep-level guard against accident, not against evasion.
py_violation=0
for f in tools/*.py; do
  [ -f "$f" ] || continue
  for mod in $(grep -hE '^[[:space:]]*(import|from)[[:space:]]+[A-Za-z_]' "$f" \
      | sed -E 's/^[[:space:]]*from[[:space:]]+([A-Za-z_][A-Za-z0-9_.]*).*/\1/' \
      | sed -E 's/^[[:space:]]*import[[:space:]]+//' \
      | tr ',' '\n' \
      | sed -E 's/^[[:space:]]*([A-Za-z_][A-Za-z0-9_]*).*/\1/' \
      | grep -E '^[A-Za-z_]' \
      | sort -u); do
    case " $PY_ALLOWED_IMPORTS " in
      *" $mod "*) ;;
      *)
        echo "check_constraints: $f imports '$mod', which is not on the stdlib allowlist"
        py_violation=1
        ;;
    esac
  done
done
if [ "$py_violation" -ne 0 ]; then
  report "a tools/*.py file imports something outside the stdlib allowlist"
fi

if [ "$fail" -ne 0 ]; then
  echo "make lint FAILED"
  exit 1
fi
echo "make lint OK"
