#!/usr/bin/env bash
# Cross-build the Windows x64 plugin (plugin.dll) in a Docker container using
# MinGW-w64, against the matching Rack-SDK. Same idea as tools/build-linux.sh:
# a pre-submission sanity build that confirms the plugin compiles for Windows
# without the heavy full rack-plugin-toolchain. The official all-platform builds
# come from the VCV Library farm on submission.
#
# The Rack SDK's arch.mk picks the target from `$(CXX) -dumpmachine`, so pointing
# CC/CXX at the mingw cross-compiler is all it takes to target Windows.
#
# Requires a Docker runtime (e.g. colima). Output: dist/plugin-win-x64.dll
set -euo pipefail

SDK_VER="${SDK_VER:-2.6.6}"
PLUGIN_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo ">> Windows x64 build (MinGW-w64) against Rack-SDK ${SDK_VER}"
docker run --rm --platform linux/amd64 -v "${PLUGIN_DIR}":/src ubuntu:22.04 bash -c "
set -e
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    build-essential wget unzip jq mingw-w64 >/dev/null
cd /tmp
wget -q https://vcvrack.com/downloads/Rack-SDK-${SDK_VER}-win-x64.zip
unzip -q Rack-SDK-${SDK_VER}-win-x64.zip
# fresh tree (no shared build/ objects from other-platform builds)
mkdir -p /work/vendor
cp -r /src/src /src/res /src/Makefile /src/plugin.json /work/
cp -r /src/vendor/svghelper /work/vendor/
cd /work
# Use the POSIX-threads MinGW variant, not the default win32-threads one: std::mutex,
# std::thread, std::condition_variable (which the Rack SDK's audio.hpp uses) only
# exist under the posix threads model. This is the same model VCV's official
# toolchain uses. -dumpmachine still reports x86_64-w64-mingw32, so the SDK's
# arch.mk detects ARCH_WIN unchanged.
make RACK_DIR=/tmp/Rack-SDK \
     CC=x86_64-w64-mingw32-gcc-posix \
     CXX=x86_64-w64-mingw32-g++-posix \
     STRIP=x86_64-w64-mingw32-strip \
     -j\$(nproc)
mkdir -p /src/dist
cp plugin.dll /src/dist/plugin-win-x64.dll
echo '>> BUILT:'
ls -la /src/dist/plugin-win-x64.dll
"
