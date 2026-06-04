# TNN1T1S Ghost — VCV Rack plugin Makefile
#
# RACK_DIR must point at a Rack plugin SDK (or a Rack source checkout's
# build tree). Override it from the environment:
#
#     make RACK_DIR=/path/to/Rack-SDK
#
# By default it points at the rack-sdk vendored in the sibling vcv-rack repo,
# so the plugin builds out of the box on this machine.
#
# Capture THIS Makefile's directory immediately with := before plugin.mk is
# included and appends to MAKEFILE_LIST. A lazy $(lastword $(MAKEFILE_LIST))
# in RACK_DIR would otherwise re-resolve to plugin.mk's path once included,
# mis-pointing RACK_DIR in a bare shell (issue: RACK_DIR self-resolution).
GHOST_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
RACK_DIR ?= $(realpath $(GHOST_MK_DIR)../vcv-rack/vendor/rack-sdk)

FLAGS += -Ivendor/svghelper -Isrc

# --- Shipping plugin: the registered kit only ---
SOURCES += src/plugin.cpp
SOURCES += src/Attenuate.cpp
SOURCES += src/GhostCtrl.cpp
SOURCES += src/Kck.cpp
SOURCES += src/Snr.cpp
SOURCES += src/ChhOhh.cpp
SOURCES += src/RimClap.cpp
SOURCES += src/Toms.cpp
SOURCES += src/CrashRide.cpp
SOURCES += src/GhostMix.cpp

# --- Lab/bench variants (per-voice tuning tools, unregistered) ---
# Excluded from the shipping build by default so the VCV Library push carries
# only the kit. Build them for dev/rot-checking with: make BUILD_LABS=1
BUILD_LABS ?= 0
ifeq ($(BUILD_LABS),1)
SOURCES += $(wildcard src/lab/*.cpp)
FLAGS += -DGHOST_LABS
endif

DISTRIBUTABLES += res

# The Ghost voices are pure DSP + procedural NanoVG panels: no Accelerate, no
# llama, no libsndfile. Only the Rack SDK is required.

include $(RACK_DIR)/plugin.mk

RACK_PLUGINS := $(HOME)/Library/Application Support/Rack2/plugins-mac-arm64

# Fast in-place deploy for development (the SDK `install` target builds and
# installs a packaged .vcvplugin instead).
deploy: all
	mkdir -p "$(RACK_PLUGINS)/tnn1t1s-ghost"
	cp plugin.dylib "$(RACK_PLUGINS)/tnn1t1s-ghost/plugin.dylib"
	cp plugin.json  "$(RACK_PLUGINS)/tnn1t1s-ghost/plugin.json"
	cp -r res/      "$(RACK_PLUGINS)/tnn1t1s-ghost/res/"

.PHONY: deploy

# --- Offline DSP stress harness (issue #17) --------------------------------
# Headless robustness / RT-safety / performance suite over the voice cores.
# Builds + runs entirely against the Rack SDK headers (no libRack, no GUI).
# See tests/stress/ and reflect/stress-test-report.md.
stress:
	$(MAKE) -C tests/stress RACK_DIR="$(RACK_DIR)" run

.PHONY: stress
