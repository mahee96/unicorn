#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$(mktemp -d)"
XCFRAMEWORK_PATH="${ROOT_DIR}/build/Unicorn.xcframework"

mkdir -p "${ROOT_DIR}/build"

UNICORN_LOGGING="${UNICORN_LOGGING:-ON}"
echo "==> Unicorn Logging: ${UNICORN_LOGGING}"

COMMON_CMAKE=(
  -DCMAKE_BUILD_TYPE=Release
  -DUNICORN_ARCH="aarch64"
  -DUNICORN_BUILD_TESTS=OFF
  -DUNICORN_INSTALL=OFF
  -DUNICORN_LOGGING="${UNICORN_LOGGING}"
)

patch_config_host() {
    local cfg="$1/config-host.h"
    if [ -f "${cfg}" ]; then
        sed -i '' 's/#define HAVE_PTHREAD_JIT_PROTECT 1/\/* HAVE_PTHREAD_JIT_PROTECT removed for non-macOS target *\//' "${cfg}"
        echo "    Patched config-host.h"
    fi
}

build_slice() {
    local name="$1"
    local sysroot="$2"
    local archs="$3"
    local extra_cflags="$4"
    local out_name="$5"

    echo "==> Building Unicorn for ${name}..."
    local build_dir="${ROOT_DIR}/build_${out_name}"
    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"
    pushd "${build_dir}" >/dev/null

    local cc_path
    cc_path="$(xcrun -sdk "${sysroot}" -find clang)"
    local cmake_args=("${COMMON_CMAKE[@]}"
        -DCMAKE_C_COMPILER="${cc_path}"
        -DCMAKE_OSX_SYSROOT="${sysroot}"
        -DCMAKE_OSX_ARCHITECTURES="${archs}"
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
    )
    if [ -n "${extra_cflags}" ]; then
        cmake_args+=(-DCMAKE_C_FLAGS="${extra_cflags}")
    fi

    cmake "${ROOT_DIR}" "${cmake_args[@]}"
    patch_config_host "${build_dir}"
    cmake --build . --config Release -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    mkdir -p "${OUTPUT_DIR}/${out_name}"
    cp libunicorn.a "${OUTPUT_DIR}/${out_name}/libunicorn.a"
    popd >/dev/null
    echo "    ${name} done."
}

# ── macOS (native JIT) ──
echo "==> Building Unicorn for macOS..."
BUILD_DIR_MAC="${ROOT_DIR}/build_mac"
rm -rf "${BUILD_DIR_MAC}"
mkdir -p "${BUILD_DIR_MAC}"
pushd "${BUILD_DIR_MAC}" >/dev/null
cmake "${ROOT_DIR}" "${COMMON_CMAKE[@]}" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build . --config Release -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
mkdir -p "${OUTPUT_DIR}/mac"
cp libunicorn.a "${OUTPUT_DIR}/mac/libunicorn.a"
popd >/dev/null
echo "    macOS done."

#  Simulators (TCI pure software interpreter)
build_slice "iOS Simulator"      iphonesimulator   "arm64;x86_64"  "-DUNICORN_ENABLE_TCI=ON"                  "ios_sim"
build_slice "tvOS Simulator"     appletvsimulator  "arm64;x86_64"  "-DUNICORN_ENABLE_TCI=ON"                  "tvos_sim"
build_slice "visionOS Simulator" xrsimulator       "arm64"         "-DUNICORN_ENABLE_TCI=ON -DCONFIG_INT128"  "xros_sim"
build_slice "iOS Device"         iphoneos          "arm64"         "-DUNICORN_ENABLE_TCI=ON"                  "ios_device"
build_slice "tvOS Device"        appletvos         "arm64"         "-DUNICORN_ENABLE_TCI=ON -DCONFIG_INT128"  "tvos_device"
build_slice "visionOS Device"    xros              "arm64"         "-DUNICORN_ENABLE_TCI=ON -DCONFIG_INT128"  "xros_device"

# ── Bundle into XCFramework
echo "==> Creating Unicorn.xcframework..."
rm -rf "${XCFRAMEWORK_PATH}"

xcodebuild -create-xcframework \
  -library "${OUTPUT_DIR}/mac/libunicorn.a" -headers "${ROOT_DIR}/include" \
  -library "${OUTPUT_DIR}/ios_sim/libunicorn.a" -headers "${ROOT_DIR}/include" \
  -library "${OUTPUT_DIR}/tvos_sim/libunicorn.a" -headers "${ROOT_DIR}/include" \
  -library "${OUTPUT_DIR}/xros_sim/libunicorn.a" -headers "${ROOT_DIR}/include" \
  -library "${OUTPUT_DIR}/ios_device/libunicorn.a" -headers "${ROOT_DIR}/include" \
  -library "${OUTPUT_DIR}/tvos_device/libunicorn.a" -headers "${ROOT_DIR}/include" \
  -library "${OUTPUT_DIR}/xros_device/libunicorn.a" -headers "${ROOT_DIR}/include" \
  -output "${XCFRAMEWORK_PATH}"

rm -rf "${OUTPUT_DIR}"

echo ""
echo "Done! XCFramework: ${XCFRAMEWORK_PATH}"
