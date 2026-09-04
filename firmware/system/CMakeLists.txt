cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(steamcore_system)

# Constitution §3: "C++17, one dialect for both the host tests and the
# target build" -- without this, the toolchain's own default (gnu++2b on
# this xtensa-esp-elf GCC) applies to every source the device compiles,
# engine .cpp's included, silently diverging from the host Makefile's
# -std=c++17 (review F5: the plan's own gate claimed this pin already
# existed; it didn't). APPEND, not REPLACE: it must land after the
# toolchain's own default -std= flag so it's the one the compiler
# actually honors (the last -std wins).
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17" APPEND)
