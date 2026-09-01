SHELL := /bin/bash

CXX := clang++
BUILD_DIR := build

STEAMCORE_DIR := firmware/steamcore
INC_DIR := $(STEAMCORE_DIR)/include
SRC_DIR := $(STEAMCORE_DIR)/src
TEST_DIR := $(STEAMCORE_DIR)/test

# Exactly one include path: the public headers. Nothing in src/ or test/
# needs an extra -I (quoted includes resolve relative to the including
# file's own directory), and this is the concrete claim that makes "no
# ESP-IDF header in logic code" compiler-enforced rather than a
# convention (plan §1 Decision 2; review F12).
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -I$(INC_DIR)

# The one literal for the committed dump fixture's path (docs/dump-format.md
# "Fixture path"). C++ gets it via -D; Python gets it via the exported env
# var below (see the `test-python`/`test-roundtrip`/`view` targets).
FIXTURE_DUMP := firmware/steamcore/test/fixtures/reference_pattern.scfb
CXXFLAGS += -DSTEAMCORE_FIXTURE_DUMP='"$(FIXTURE_DUMP)"'

ENGINE_SRCS := $(wildcard $(SRC_DIR)/*.cpp)
TEST_SRCS := $(wildcard $(TEST_DIR)/*_test.cpp)
HARNESS_SRCS := $(TEST_DIR)/test_harness.cpp $(TEST_DIR)/test_main.cpp

TEST_BIN := $(BUILD_DIR)/steamcore_tests
SELFCHECK_BIN := $(BUILD_DIR)/steamcore_selfcheck
ASAN_BIN := $(BUILD_DIR)/steamcore_tests_asan
BENCH_BIN := $(BUILD_DIR)/steamcore_bench

# Every binary target below is itself .PHONY: its recipe runs on EVERY
# invocation, unconditionally, regardless of any file mtime. This host
# has only GNU Make 3.81, which compares timestamps at one-second
# granularity and treats "equal" as up to date -- an edit landing in the
# same second as the previous link was invisible to a normal
# prerequisite-based rule (review F11, found while re-verifying F1's
# header-dependency fix: 4 false greens in 10 trials). A full rebuild
# takes ~1.3s, which is cheap enough that giving up incremental caching
# entirely is the right trade for a gate that must never report success
# on code it did not actually just compile.
.PHONY: $(TEST_BIN) $(SELFCHECK_BIN) $(ASAN_BIN) $(BENCH_BIN)

.PHONY: test
test: $(TEST_BIN)
	$(TEST_BIN) $(FILTER)

$(TEST_BIN):
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -O2 -o $@ $(ENGINE_SRCS) $(TEST_SRCS) $(HARNESS_SRCS)

# Proves the harness itself fails loudly: builds a binary containing one
# deliberate CHECK failure and asserts (a) it exits non-zero and (b) its
# output names the failing test and its file:line (AC-1.2). This target
# exits 0 only if the harness behaved correctly under a real failure.
.PHONY: test-negative
test-negative: $(SELFCHECK_BIN) $(TEST_BIN)
	@echo "--- running deliberately-failing suite (expected to fail) ---"; \
	output="$$($(SELFCHECK_BIN) 2>&1)"; \
	status=$$?; \
	echo "$$output"; \
	if [ $$status -eq 0 ]; then \
		echo "test-negative FAILED: selfcheck binary exited 0, expected non-zero"; \
		exit 1; \
	fi; \
	if ! echo "$$output" | grep -q "selfcheck_deliberate_failure"; then \
		echo "test-negative FAILED: output does not name the failing test"; \
		exit 1; \
	fi; \
	if ! echo "$$output" | grep -qE "harness_selfcheck\.cpp:[0-9]+"; then \
		echo "test-negative FAILED: output does not contain a file:line location"; \
		exit 1; \
	fi; \
	echo "test-negative OK: harness correctly reported the deliberate failure"
	@echo "--- verifying a zero-match filter is treated as failure (review F2) ---"; \
	if $(TEST_BIN) this_filter_matches_nothing_zzz; then \
		echo "test-negative FAILED: zero-match filter exited 0, expected non-zero"; \
		exit 1; \
	fi; \
	echo "test-negative OK: zero-match filter correctly reported as failure"

$(SELFCHECK_BIN):
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -O2 -o $@ $(TEST_DIR)/harness_selfcheck.cpp $(HARNESS_SRCS)

ASAN_FLAGS := -fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g

# -fno-sanitize-recover=all is mandatory: plain UBSan prints a finding and
# still exits 0, which would make this target silently green (plan §1
# Decision 3).
.PHONY: test-asan
test-asan: $(ASAN_BIN)
	$(ASAN_BIN) $(FILTER)

$(ASAN_BIN):
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(ASAN_FLAGS) -o $@ $(ENGINE_SRCS) $(TEST_SRCS) $(HARNESS_SRCS)

# Real GCC coverage is unverified on this host — /usr/bin/g++ is Apple
# clang (see docs/host-tests.md). CXX is overridable so a genuine GCC can
# be pointed at the suite the moment one exists.
.PHONY: test-gcc
test-gcc:
	$(MAKE) test CXX=g++

.PHONY: bench
bench: $(BENCH_BIN)
	$(BENCH_BIN)

$(BENCH_BIN):
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -O2 -o $@ $(ENGINE_SRCS) $(TEST_DIR)/bench_dirty_scan.cpp

VIEWER_PNG := $(BUILD_DIR)/pattern.png

# US-5: one command from a clean checkout to a viewable PNG of the fixture
# pattern. Depends on `test` (not just $(TEST_BIN)) because the fixture
# is committed to git and therefore survives `make clean` -- linking the
# binary without RUNNING it would silently view last commit's fixture
# instead of what the current source tree actually draws.
.PHONY: view
view: test
	@mkdir -p $(BUILD_DIR)
	python3 -B tools/fb_view.py $(FIXTURE_DUMP) $(VIEWER_PNG)

# Independent PNG-validity oracle (plan §1 Decision, risk R1/R2): confirms
# a decoder that shares no code with fb_view.py's own reader can open the
# file and agrees on its declared pixel dimensions. This reads only the
# IHDR chunk -- it does NOT decode IDAT, so it is not a general "is this
# PNG's pixel data correct" check (review F3); that proof is
# test-roundtrip's job. macOS-only; degrades to an explicit SKIPPED
# elsewhere rather than a silent pass (constitution's honest-status rule).
# Depends on `test`, not just $(TEST_BIN), for the same reason `view`
# does (review F6): the fixture is committed to git and survives `make
# clean`, so linking without running would validate a stale PNG decoded
# from last commit's fixture instead of the current source tree.
.PHONY: test-png-external
test-png-external: test
	@if ! command -v sips >/dev/null 2>&1; then \
		echo "test-png-external SKIPPED (sips not found)"; \
		exit 0; \
	fi; \
	set -e; \
	mkdir -p $(BUILD_DIR); \
	rm -f $(BUILD_DIR)/sips_check.png; \
	python3 -B tools/fb_view.py $(FIXTURE_DUMP) $(BUILD_DIR)/sips_check.png > /dev/null; \
	want_w=$$(python3 -B -c "import struct; d=open('$(FIXTURE_DUMP)','rb').read(); print(struct.unpack_from('<H', d, 6)[0])"); \
	want_h=$$(python3 -B -c "import struct; d=open('$(FIXTURE_DUMP)','rb').read(); print(struct.unpack_from('<H', d, 8)[0])"); \
	got_w=$$(sips -g pixelWidth $(BUILD_DIR)/sips_check.png | awk '/pixelWidth/ {print $$2}'); \
	got_h=$$(sips -g pixelHeight $(BUILD_DIR)/sips_check.png | awk '/pixelHeight/ {print $$2}'); \
	if [ "$$want_w" != "$$got_w" ] || [ "$$want_h" != "$$got_h" ]; then \
		echo "test-png-external FAILED: sips reports $${got_w}x$${got_h}, dump declares $${want_w}x$${want_h}"; \
		exit 1; \
	fi; \
	echo "test-png-external OK: sips independently confirms $${got_w}x$${got_h}"

.PHONY: test-python
test-python:
	STEAMCORE_FIXTURE_DUMP=$(FIXTURE_DUMP) python3 -B -m unittest discover -s tools -p 'test_*.py' -v

# AC-4: needs no C++ toolchain -- reads the already-committed fixture.
.PHONY: test-roundtrip
test-roundtrip:
	STEAMCORE_FIXTURE_DUMP=$(FIXTURE_DUMP) python3 -B -m unittest tools.test_roundtrip -v

.PHONY: lint
lint:
	@bash tools/check_constraints.sh

.PHONY: test-all
test-all:
	$(MAKE) test
	$(MAKE) test-negative
	$(MAKE) test-asan
	$(MAKE) test-gcc
	$(MAKE) bench
	$(MAKE) test-python
	$(MAKE) test-roundtrip
	$(MAKE) test-png-external
	$(MAKE) lint

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
