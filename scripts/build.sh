#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
target="all"
target_set=false
use_docker=false
output_dir="${project_dir}/dist"

usage() {
    echo "usage: ${0} [--docker] [--output DIR] [all|pico1|pico2|rp2040zero]"
    echo "Pico SDK board names pico and waveshare_rp2040_zero are also accepted."
}

while [[ "$#" -gt 0 ]]; do
    case "${1}" in
        --docker)
            use_docker=true
            ;;
        --output)
            if [[ "$#" -lt 2 || -z "${2}" || "${2}" == -* ]]; then
                echo "error: --output requires a directory." >&2
                exit 2
            fi
            output_dir="${2}"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            usage >&2
            exit 2
            ;;
        *)
            if ${target_set}; then
                usage >&2
                exit 2
            fi
            target="${1}"
            target_set=true
            ;;
    esac
    shift
done

case "${target}" in
    all)
        boards=(pico1 pico2 rp2040zero)
        ;;
    pico|pico1)
        target="pico1"
        boards=(pico1)
        ;;
    pico2)
        boards=(pico2)
        ;;
    waveshare_rp2040_zero|rp2040zero)
        target="rp2040zero"
        boards=(rp2040zero)
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

if ${use_docker}; then
    if ! command -v docker >/dev/null 2>&1; then
        echo "error: Docker with BuildKit is required for --docker." >&2
        exit 1
    fi

    docker_args=(--build-arg "BOARD=${target}")
    for name in PICO_SDK_VERSION PICO_SDK_REPO GHOST_USB_SERIAL CMAKE_BUILD_PARALLEL_LEVEL; do
        if [[ -n "${!name:-}" ]]; then
            docker_args+=(--build-arg "${name}=${!name}")
        fi
    done

    docker build --target firmware "${docker_args[@]}" --output "${output_dir}" "${project_dir}"
    exit 0
fi

if [[ -z "${PICO_TOOLCHAIN_PATH:-}" ]] && ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "error: arm-none-eabi-gcc was not found." >&2
    echo "Install the shared build dependencies on Ubuntu/Debian:" >&2
    echo "  sudo ${script_dir}/install_build_deps.sh" >&2
    exit 1
fi

if [[ -z "${PICO_SDK_PATH:-}" ]]; then
    export PICO_SDK_PATH="${project_dir}/deps/pico-sdk"
    if [[ ! -f "${PICO_SDK_PATH}/pico_sdk_init.cmake" ]]; then
        "${script_dir}/fetch_sdk.sh"
    fi
fi

cmake_args=()
if [[ -n "${GHOST_USB_SERIAL:-}" ]]; then
    cmake_args+=("-DGHOST_USB_SERIAL=${GHOST_USB_SERIAL}")
fi
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

for build_name in "${boards[@]}"; do
    case "${build_name}" in
        pico1) board=pico ;;
        pico2) board=pico2 ;;
        rp2040zero) board=waveshare_rp2040_zero ;;
    esac
    build_dir="${project_dir}/build/${build_name}"
    local_toolchain_dir="${project_dir}/deps/toolchain"

    if [[ -f "${build_dir}/CMakeCache.txt" ]] && grep -Fq "${local_toolchain_dir}" "${build_dir}/CMakeCache.txt"; then
        echo "error: ${build_dir}/CMakeCache.txt is pinned to the old local deps/toolchain compiler." >&2
        echo "Install dependencies with sudo ${script_dir}/install_build_deps.sh, then regenerate the build directory:" >&2
        echo "  cmake -E rm -rf ${build_dir}" >&2
        echo "  ${0} ${build_name}" >&2
        exit 1
    fi

    cmake -S "${project_dir}" -B "${build_dir}" -DPICO_BOARD="${board}" "${cmake_args[@]}"
    cmake --build "${build_dir}"

    mkdir -p "${output_dir}"
    cp "${build_dir}/PicoGhostHID.uf2" "${output_dir}/PicoGhostHID-${build_name}.uf2"
    cp "${build_dir}/PicoGhostHID.elf" "${output_dir}/PicoGhostHID-${build_name}.elf"
    echo "Built: ${build_dir}/PicoGhostHID.uf2"
done

cp "${project_dir}/LICENSE" "${output_dir}/LICENSE.txt"
cp "${project_dir}/LICENSES/BSD-3-Clause-ws2812.txt" "${output_dir}/BSD-3-Clause-ws2812.txt"
echo "Firmware and license notices exported to: ${output_dir}"
