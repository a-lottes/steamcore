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
# Defined here, not only where the display-driver port-scoped rules start
# further down, so the input-driver determinism block above them can
# also build a port/esp32 path -- its own existence is still verified by
# the guarded loop further down, this is only the string itself.
PORT_DIR="firmware/steamcore/port"

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

# Shared by every feature-scoped determinism-mechanism lint block below
# (game-loop, game-state, and any future one) so the token lists cannot
# drift apart between copies -- extending one to close a gap (review
# F11: the original game-loop list missed `clock_gettime`/`<time.h>`)
# extends all of them.
CLOCK_RNG_PATTERN='<chrono>|<ctime>|<time\.h>|<sys/time\.h>|std::chrono|steady_clock|system_clock|high_resolution_clock|\bclock[[:space:]]*\(|\bclock_gettime[[:space:]]*\(|\btime[[:space:]]*\(|gettimeofday|\brand[[:space:]]*\(|\bsrand[[:space:]]*\(|random_device|<random>'
SCOPED_ALLOC_PATTERN='\bnew\b|\b(m|c|re)alloc[[:space:]]*\(|\bstrdup[[:space:]]*\(|std::(vector|string|map|deque|list|function|unique_ptr|make_unique|shared_ptr|make_shared)\b'

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
  if grep -nHE "$CLOCK_RNG_PATTERN" \
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
  if grep -nHE "$SCOPED_ALLOC_PATTERN" \
      "$f" | grep -vE ':[0-9]+:[[:space:]]*//'; then
    report "dynamic allocation or a forbidden container/string/smart-pointer type was found above, in the game-loop file set"
  fi
done

echo "--- no wall-clock read or unseeded RNG in the game-state mechanism (AC-5.3) ---"
# Same reasoning as the game-loop block above, same shared pattern: a
# GameSession transition must be a pure function of its own prior state
# and the step's input, or the replay determinism proof (AC-5.1) means
# nothing.
GAME_STATE_DETERMINISM_FILES="$INCLUDE_DIR/steamcore/game_state.h $SRC_DIR/game_state.cpp $TEST_DIR/game_state_test.cpp $TEST_DIR/game_state_determinism_test.cpp $TEST_DIR/session_replay_fixture.h"
for f in $GAME_STATE_DETERMINISM_FILES; do
  if [ ! -f "$f" ]; then
    report "expected game-state file '$f' does not exist -- refusing to skip it silently"
    continue
  fi
  if grep -nHE "$CLOCK_RNG_PATTERN" \
      "$f" | grep -vE ':[0-9]+:[[:space:]]*//'; then
    report "a wall-clock read or unseeded RNG call was found above, in a file the determinism guarantee (AC-5.1) depends on"
  fi
done

echo "--- no dynamic allocation in the game-state test file set (NFR-2, extends the include/src grep to test/) ---"
# game_state.h/.cpp already live in include/+src/, so the general
# allocation check above already reaches them; this loop covers only
# the test/ files it does not reach. Same missing-file posture as above.
GAME_STATE_TEST_FILES="$TEST_DIR/game_state_test.cpp $TEST_DIR/game_state_determinism_test.cpp $TEST_DIR/session_replay_fixture.h"
for f in $GAME_STATE_TEST_FILES; do
  if [ ! -f "$f" ]; then
    report "expected game-state file '$f' does not exist -- refusing to skip it silently"
    continue
  fi
  if grep -nHE "$SCOPED_ALLOC_PATTERN" \
      "$f" | grep -vE ':[0-9]+:[[:space:]]*//'; then
    report "dynamic allocation or a forbidden container/string/smart-pointer type was found above, in the game-state test file set"
  fi
done

echo "--- no wall-clock read or unseeded RNG in the input mechanism, including its port/ (input-driver NFR-3) ---"
# Same shared CLOCK_RNG_PATTERN as the game-loop/game-state blocks above,
# but scoped differently: this is the one mechanism-lint block that DOES
# reach into port/ (gpio_input_source.{h,cpp}, added here once T9 builds
# it), the opposite of the display-driver rule below, which deliberately
# stays out of port/esp32 (NFR-5 note further down). The reason is what
# each side of port/ actually feeds: ili9488_display.cpp's SPI wait is
# display-OUTPUT timing, decoupled from game-logic determinism -- but
# GpioInputSource's whole job is to hand InputReader the raw level that
# becomes GameInput, which feeds game logic directly. A clock or RNG read
# anywhere on that path, including inside the port implementation, would
# break the same replay-determinism guarantee (constitution §3/§4, NFR-3)
# the game-loop/game-state blocks above already guard -- input has no
# "output-only" side to exempt.
INPUT_DETERMINISM_FILES="$INCLUDE_DIR/steamcore/input.h $TEST_DIR/input_test.cpp $TEST_DIR/input_session_test.cpp $TEST_DIR/fake_input_source.h $PORT_DIR/esp32/gpio_input_source.h $PORT_DIR/esp32/gpio_input_source.cpp"
# port/esp32/gpio_input_source.{h,cpp} added by T9 (they did not exist
# when T7 first wrote this block -- see the plan.md T7 deviation note).
for f in $INPUT_DETERMINISM_FILES; do
  if [ ! -f "$f" ]; then
    report "expected input-driver file '$f' does not exist -- refusing to skip it silently"
    continue
  fi
  if grep -nHE "$CLOCK_RNG_PATTERN" \
      "$f" | grep -vE ':[0-9]+:[[:space:]]*//'; then
    report "a wall-clock read or unseeded RNG call was found above, in a file the input-driver determinism guarantee (NFR-3) depends on"
  fi
done

echo "--- no dynamic allocation in the input mechanism file set (NFR-2, extends the include/src grep to test/) ---"
for f in $INPUT_DETERMINISM_FILES; do
  if [ ! -f "$f" ]; then
    report "expected input-driver file '$f' does not exist -- refusing to skip it silently"
    continue
  fi
  if grep -nHE "$SCOPED_ALLOC_PATTERN" \
      "$f" | grep -vE ':[0-9]+:[[:space:]]*//'; then
    report "dynamic allocation or a forbidden container/string/smart-pointer type was found above, in the input-driver file set"
  fi
done

echo "--- GameInput's seven-field size guard is present (input-driver NFR-8) ---"
# The size guard itself lives in game_loop.h (T1) -- this only checks it
# has not been silently deleted, the same "presence, not correctness"
# posture as the game-state enumerator/cast checks above.
if [ ! -f "$INCLUDE_DIR/steamcore/game_loop.h" ]; then
  report "expected game-loop file '$INCLUDE_DIR/steamcore/game_loop.h' does not exist -- refusing to skip it silently"
else
  if ! grep -qE 'static_assert[[:space:]]*\([[:space:]]*sizeof[[:space:]]*\([[:space:]]*GameInput[[:space:]]*\)[[:space:]]*==[[:space:]]*7[[:space:]]*\*[[:space:]]*sizeof[[:space:]]*\([[:space:]]*bool[[:space:]]*\)' \
      "$INCLUDE_DIR/steamcore/game_loop.h"; then
    report "the static_assert(sizeof(GameInput) == 7 * sizeof(bool)) size guard was not found in game_loop.h -- it must not be deleted silently"
  fi
fi

echo "--- no integer standing in for a GameState (NFR-4) ---"
# GameState's whole point is that a state is named, never a number. An
# enumerator given an explicit value, or a static_cast into/out of the
# enum, would let a digit stand in for a state again, defeating the
# -Wswitch exhaustiveness check the type otherwise gets for free (T2).
if [ ! -f "$INCLUDE_DIR/steamcore/game_state.h" ]; then
  report "expected game-state file '$INCLUDE_DIR/steamcore/game_state.h' does not exist -- refusing to skip it silently"
else
  if grep -nHE '\b(READY|PLAYING|GAME_OVER)[[:space:]]*=[[:space:]]*[0-9]' \
      "$INCLUDE_DIR/steamcore/game_state.h" | grep -vE ':[0-9]+:[[:space:]]*//'; then
    report "a GameState enumerator was given an explicit value above -- states are named, never numbered"
  fi
  if grep -nHE 'static_cast<[[:space:]]*GameState[[:space:]]*>' \
      "$INCLUDE_DIR/steamcore/game_state.h" | grep -vE ':[0-9]+:[[:space:]]*//'; then
    report "a static_cast<GameState> was found above -- a state is reached only through GameSession::advance"
  fi
fi

SYSTEM_MAIN_DIR="firmware/system/main"
DISPLAY_DRIVER_LITERAL_FILES="$INCLUDE_DIR/steamcore/panel_format.h $SRC_DIR/panel_format.cpp $INCLUDE_DIR/steamcore/tile_pusher.h"

# Same missing-path posture as the directory guard at the top of this
# script and the game-loop/game-state file loops (reviews F15/F6): a grep
# against a path that does not exist fails with "No such file or
# directory", the surrounding `if` reads that as "no match", and the
# script would print `make lint OK` having scanned nothing. Verified at
# review: with firmware/steamcore/port/ moved away, all three port-scoped
# rules below passed silently and the script exited 0.
for d in "$PORT_DIR" "$SYSTEM_MAIN_DIR"; do
  if [ ! -d "$d" ]; then
    report "expected display-driver directory '$d' (relative to repo root) does not exist -- refusing to skip its checks silently"
  fi
done
for f in $DISPLAY_DRIVER_LITERAL_FILES; do
  if [ ! -f "$f" ]; then
    report "expected display-driver file '$f' does not exist -- refusing to skip it silently"
  fi
done

echo "--- no #include from port/ inside include/ or src/ (display-driver, constitution §4) ---"
# port/esp32 is the ONLY directory under firmware/steamcore allowed to
# include an ESP-IDF header -- true today by construction, since the
# ESP-IDF-header check above already scans include/+src/ and port/ is the
# one directory it never reaches. This is the other half of that same
# boundary: the host-tested tree must never reach INTO port/ either, or a
# single #include would drag an ESP-IDF dependency back across the wall
# the whole port/ split exists to build (display-driver plan §1 Decision 1).
if grep -rnE '#include[[:space:]]*[<"]port/' "$INCLUDE_DIR" "$SRC_DIR"; then
  report "a #include referencing port/ was found inside include/ or src/ -- the host-tested tree must never reach into the ESP-IDF-only driver"
fi

echo "--- no GPIO literal outside board_config.h (display-driver, constitution §6, first automated) ---"
# First automation of constitution §6's non-negotiable ("No GPIO number
# outside board_config.h") -- every prior increment left this to reviewer
# diligence. A line naming a gpio/spi/io_num/pin concept AND carrying one
# of this board's actual pin numbers (4-7, 9-15, 17-18, board_config.h) as
# a bare literal is exactly the failure mode board_config.h exists to
# prevent: a second, drifting copy of a pin assignment. board_config.h
# itself is where these numbers are legitimately defined, so it's
# excluded. input-driver (T7) extended the digit set from 9-14 (display
# only) to also cover 4/5/6/7/15/17/18 (input); re-run against the
# unmodified tree with the extended set first to confirm zero false
# positives, per this rule's own "narrow the token pattern, never drop a
# digit" posture.
GPIO_TOKEN_PATTERN='(gpio|spi|io_num|[Pp]in)'
# The digit match runs against the LINE CONTENT only, and treats `_` as a
# token separator. Both were found wrong at review, in opposite
# directions:
#  - `grep -rn` prefixes every hit with `path:lineno:`, so matching the
#    whole line made any line numbered 9-14 that merely names a
#    gpio/spi/pin concept a false positive -- reproduced with a file
#    whose line 10 read `int spiDummy = 0;` and contained no pin literal.
#  - `\b10\b` never matches ESP-IDF's own canonical spelling
#    `GPIO_NUM_10` (no word boundary after `_`), which is the single most
#    likely way a stray pin literal actually appears in port/ --
#    reproduced with `gpio_set_level(GPIO_NUM_10, 1)`, which passed clean.
# awk, not grep, because the match has to be made against the stripped
# text while the report still names the file and the original line -- the
# same reason the glyph-metric check above uses awk.
if grep -rnE --include='*.h' --include='*.cpp' "$GPIO_TOKEN_PATTERN" \
    "$INCLUDE_DIR" "$SRC_DIR" "$PORT_DIR" "$SYSTEM_MAIN_DIR" \
    | grep -v '/board_config\.h:' \
    | grep -vE ':[0-9]+:[[:space:]]*//' \
    | awk '
      {
        content = $0
        sub(/^[^:]*:[0-9]+:/, "", content)
        if (content ~ /(^|[^0-9A-Za-z])(4|5|6|7|9|10|11|12|13|14|15|17|18)([^0-9A-Za-z]|$)/) {
          print
          found = 1
        }
      }
      END { exit(found ? 0 : 1) }
    '; then
  report "a GPIO pin literal (4/5/6/7/9/10/11/12/13/14/15/17/18) was found near a gpio/spi/io_num/pin token outside board_config.h"
fi

echo "--- no resolution/tile-size literal in port/esp32 (display-driver, extends the include/src scan) ---"
# The resolution- and tile-size-literal checks above only scan include/+
# src/, so they never reach port/esp32 -- this extends the same rule
# there. panel_format.{h,cpp}/tile_pusher.h already fall under the
# general include/+src/ scan; this block is specifically the coverage
# gap those checks structurally cannot close on their own.
if grep -rnE --include='*.h' --include='*.cpp' '\b(240|160|480|320|16)\b' \
    "$PORT_DIR" \
    | grep -vE ':[0-9]+:[[:space:]]*//'; then
  report "a resolution or tile-size literal (240/160/480/320/16) was found in port/esp32, outside config.h"
fi

echo "--- no bare scale-factor literal in the display-driver pixel/tile math (AC-2.2) ---"
# panel_format.{h,cpp} and tile_pusher.h are exactly the files whose
# whole job is the x2 engine-to-panel mapping -- a "* 2" or "2 *"
# anywhere in their logic would be an unnamed second copy of kPanelScale
# that could silently drift from config.h's definition if the panel's
# scale factor ever changes (constitution §3 "Open (Phase 4)"). Scoped to
# multiplication specifically, not every bare "2": panel_format.cpp
# legitimately indexes individual RGB byte offsets (out[index + 2] for
# the blue channel), which is unrelated to the scale factor and would be
# a false positive under a broader "any bare 2" pattern.
if grep -rnE --include='*.h' --include='*.cpp' '([*][[:space:]]*2\b)|(\b2[[:space:]]*[*])' \
    $DISPLAY_DRIVER_LITERAL_FILES \
    | grep -vE ':[0-9]+:[[:space:]]*//'; then
  report "a bare scale-factor multiplication (*2 or 2*) was found in the display-driver pixel/tile math, outside config.h's kPanelScale"
fi

echo "--- no full-frame SPI transaction: spi_device_transmit stays inside port/esp32/ili9488_display.cpp only (AC-3.3) ---"
# The real guard against a full-frame push is structural (plan §1
# Decision 4: TilePusher's Transmitter concept only ever offers one tile
# at a time) -- this lint rule guards the OTHER way a full-frame push
# could reappear: a second, ad-hoc SPI call site outside the one file
# that owns hardware transmission, the same shape the deleted bring-up
# spike's own scanline-fill function was. Recorded interpretation: this
# checks *where* spi_device_transmit is called from, not the byte count
# of any one call -- a transfer-size check would need to parse C++
# expressions, which grep cannot do reliably.
if grep -rlE '\bspi_device_(transmit|polling_transmit)[[:space:]]*\(' \
    "$INCLUDE_DIR" "$SRC_DIR" "$PORT_DIR" "$SYSTEM_MAIN_DIR" \
    | grep -v '/port/esp32/ili9488_display\.cpp$'; then
  report "spi_device_transmit/spi_device_polling_transmit was called outside port/esp32/ili9488_display.cpp -- the display driver's SPI transmission must have exactly one call site in the codebase"
fi

echo "--- no dynamic allocation in port/esp32 (display-driver, NFR-2, extends the include/src grep) ---"
if grep -rnE "$SCOPED_ALLOC_PATTERN" \
    "$PORT_DIR" \
    | grep -vE ':[0-9]+:[[:space:]]*//'; then
  report "dynamic allocation or a forbidden container/string/smart-pointer type was found in port/esp32"
fi

# Note (NFR-5): unlike the game-loop/game-state blocks above, this
# feature does NOT get a clock/RNG ban in port/esp32 -- waiting for an
# SPI DMA transfer to complete is display-OUTPUT timing, decoupled from
# and never feeding back into the 60Hz game-logic determinism guarantee
# (spec NFR-5). US-5's clock-speed tuning (a later task) also legitimately
# measures elapsed time there. The pixel-conversion/tile-mapping math
# (panel_format.{h,cpp}, tile_pusher.h) IS covered -- it lives in
# include/+src/, so the project-wide allocation check above already
# reaches it, and it has no timing/RNG dependency to guard in the first
# place.

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
