# tests/stress/Makefile -- offline DSP stress harness for the Ghost voices.
#
# Builds two binaries from one source:
#   stress_san  -- ASan+UBSan correctness build (robustness invariants, leaks)
#   stress_perf -- -O2 perf build (robustness + zero-alloc + ns/sample + RSS)
#
# Invoked by the top-level `make stress` target. Header-only against the Rack
# SDK; no libRack link. A tiny shim (rack_shim.cpp) satisfies the NanoVG color
# symbols dragged in by componentlibrary.hpp.

RACK_DIR ?= $(realpath $(dir $(lastword $(MAKEFILE_LIST)))../../../vcv-rack/vendor/rack-sdk)

SRC_DIR  := $(dir $(lastword $(MAKEFILE_LIST)))
REPO     := $(SRC_DIR)../..

INC := -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include -I$(REPO)/src -I$(REPO)/vendor/svghelper
STD := -std=c++11
WARN := -Wall

SAN_FLAGS  := -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1
PERF_FLAGS := -O2 -DSTRESS_PERF

COMMON := $(STD) $(WARN) $(INC)

.PHONY: all run clean spike

all: stress_san stress_perf

stress_san: stress_test.cpp voices.hpp rack_shim.cpp
	$(CXX) $(COMMON) $(SAN_FLAGS) stress_test.cpp rack_shim.cpp -o $@

stress_perf: stress_test.cpp voices.hpp rack_shim.cpp
	$(CXX) $(COMMON) $(PERF_FLAGS) stress_test.cpp rack_shim.cpp -o $@

spike: spike_kck.cpp rack_shim.cpp
	$(CXX) $(COMMON) -O1 spike_kck.cpp rack_shim.cpp -o $@

run: all
	@echo "=================== PERF BUILD ==================="; ./stress_perf;  echo "perf exit: $$?"
	@echo "=================== SANITIZED BUILD =============="; UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 ./stress_san; echo "san exit: $$?"
	@echo "(LSan leak detection is unavailable on Darwin/arm64; RSS-flatness in the perf build is the leak check.)"

clean:
	rm -f stress_san stress_perf spike
