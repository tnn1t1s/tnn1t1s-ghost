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

# --- Layer 4 (libFuzzer) ----------------------------------------------------
# Apple clang ships no libFuzzer; use an LLVM clang++ that does. Override on the
# command line if your LLVM lives elsewhere:  make fuzz FUZZ_CXX=/path/clang++
# In CI (Linux) the system clang++ already has libFuzzer, so the default falls
# through to plain `clang++`.
FUZZ_CXX   ?= $(firstword $(wildcard /opt/homebrew/opt/llvm/bin/clang++ /usr/local/opt/llvm/bin/clang++) clang++)
FUZZ_FLAGS := -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -g -O1
# Homebrew LLVM's libFuzzer.a is built against LLVM's own libc++; link it (and
# rpath it) so its std::__1 symbols resolve. Harmless/no-op with system clang in
# CI, where this directory simply doesn't exist (the wildcard expands to empty).
FUZZ_LIBCXX := $(firstword $(wildcard /opt/homebrew/opt/llvm/lib/c++ /usr/local/opt/llvm/lib/c++))
comma := ,
FUZZ_LDFLAGS := $(if $(FUZZ_LIBCXX),-L$(FUZZ_LIBCXX) -Wl$(comma)-rpath$(comma)$(FUZZ_LIBCXX))
# Bounded campaign knob (wall-clock seconds for the whole run). Override freely:
#   make fuzz FUZZ_TIME=60
FUZZ_TIME  ?= 30

.PHONY: all run clean spike fuzz fuzz-build

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

fuzz-build: fuzz_voice.cpp voices.hpp rack_shim.cpp
	$(FUZZ_CXX) $(COMMON) $(FUZZ_FLAGS) fuzz_voice.cpp rack_shim.cpp $(FUZZ_LDFLAGS) -o fuzz_voice

# Bounded coverage-guided campaign. libFuzzer's first byte selects the voice, so
# a single run exercises all 10 cores; the wall-clock cap keeps CI deterministic.
fuzz: fuzz-build
	@mkdir -p corpus
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
	  ./fuzz_voice corpus -max_total_time=$(FUZZ_TIME) -print_final_stats=1

clean:
	rm -f stress_san stress_perf spike fuzz_voice
	rm -rf fuzz_voice.dSYM
