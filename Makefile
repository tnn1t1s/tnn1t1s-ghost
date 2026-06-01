# TNN1T1S Ghost — VCV Rack plugin Makefile
#
# RACK_DIR must point at a Rack plugin SDK (or a Rack source checkout's
# build tree). Override it from the environment:
#
#     make RACK_DIR=/path/to/Rack-SDK
#
# By default it points at the rack-sdk vendored in the sibling vcv-rack repo,
# so the plugin builds out of the box on this machine.
RACK_DIR ?= $(realpath $(dir $(lastword $(MAKEFILE_LIST)))../vcv-rack/vendor/rack-sdk)

FLAGS += -Ivendor/svghelper

SOURCES += src/plugin.cpp
SOURCES += src/Attenuate.cpp
SOURCES += src/Tr909Ctrl.cpp
SOURCES += src/Kck.cpp
SOURCES += src/Snr.cpp
SOURCES += src/ChhOhh.cpp
SOURCES += src/RimClap.cpp
SOURCES += src/Toms.cpp
SOURCES += src/CrashRide.cpp
SOURCES += src/GhostMix.cpp

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
