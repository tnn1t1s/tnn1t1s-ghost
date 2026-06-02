#!/usr/bin/env bash
# Cross-build the Linux x64 plugin (plugin.so) in a Docker container, against the
# matching Rack-SDK. Verifies the plugin compiles on Linux without the heavy full
# rack-plugin-toolchain (which needs Xcode + hours of image builds). The official
# all-platform builds come from the VCV Library farm on submission; this is a
# pre-submission sanity build.
#
# Requires a Docker runtime (e.g. colima). Output: dist/plugin-lin-x64.so
set -euo pipefail

SDK_VER="${SDK_VER:-2.6.6}"
PLUGIN_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo ">> Linux x64 build against Rack-SDK ${SDK_VER} (amd64 container)"
docker run --rm --platform linux/amd64 -v "${PLUGIN_DIR}":/src ubuntu:22.04 bash -c "
set -e
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    build-essential wget unzip jq libgl1-mesa-dev libglu1-mesa-dev >/dev/null
cd /tmp
wget -q https://vcvrack.com/downloads/Rack-SDK-${SDK_VER}-lin-x64.zip
unzip -q Rack-SDK-${SDK_VER}-lin-x64.zip
# fresh tree (no shared build/ objects from the macOS build)
mkdir -p /work/vendor
cp -r /src/src /src/res /src/Makefile /src/plugin.json /work/
cp -r /src/vendor/svghelper /work/vendor/
cd /work
make RACK_DIR=/tmp/Rack-SDK -j\$(nproc)
mkdir -p /src/dist
cp plugin.so /src/dist/plugin-lin-x64.so
echo '>> BUILT:'
ls -la /src/dist/plugin-lin-x64.so
"
