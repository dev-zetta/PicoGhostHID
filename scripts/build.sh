#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
build_dir="${project_dir}/build"
local_sdk_dir="${project_dir}/deps/pico-sdk"
local_toolchain_dir="${project_dir}/deps/toolchain"

if [[ -f "${build_dir}/CMakeCache.txt" ]] && grep -Fq "${local_toolchain_dir}" "${build_dir}/CMakeCache.txt"; then
    echo "error: build/CMakeCache.txt is pinned to the old local deps/toolchain compiler." >&2
    echo "Install gcc-arm-none-eabi system-wide, then regenerate the build directory:" >&2
    echo "  cmake -E rm -rf build" >&2
    echo "  ./scripts/build.sh" >&2
    exit 1
fi

if [[ -z "${PICO_TOOLCHAIN_PATH:-}" ]] && ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "error: arm-none-eabi-gcc was not found." >&2
    echo "Install the ARM embedded compiler system-wide, for example:" >&2
    echo "  sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi libnewlib-dev" >&2
    exit 1
fi

if [[ -z "${PICO_SDK_PATH:-}" ]]; then
    if [[ -d "${local_sdk_dir}" ]]; then
        export PICO_SDK_PATH="${local_sdk_dir}"
    else
        echo "error: PICO_SDK_PATH is not set and no local SDK checkout exists." >&2
        echo "Fetch the SDK first:" >&2
        echo "  ./scripts/fetch_sdk.sh" >&2
        echo "Or set it to an existing pico-sdk checkout:" >&2
        echo "  export PICO_SDK_PATH=/path/to/pico-sdk" >&2
        exit 1
    fi
fi

cmake_args=()
if [[ -n "${GHOST_USB_SERIAL:-}" ]]; then
    cmake_args+=("-DGHOST_USB_SERIAL=${GHOST_USB_SERIAL}")
fi

cmake -S "${project_dir}" -B "${build_dir}" "${cmake_args[@]}"
cmake --build "${build_dir}"

echo
echo "Built: ${build_dir}/PicoGhostHID.uf2"
