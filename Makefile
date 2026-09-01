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
	$(MAKE) lint

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
