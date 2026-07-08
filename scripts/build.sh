#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
build_dir="${project_dir}/build"

if [[ -z "${PICO_SDK_PATH:-}" ]]; then
    echo "error: PICO_SDK_PATH is not set." >&2
    echo "Set it to your local pico-sdk checkout, for example:" >&2
    echo "  export PICO_SDK_PATH=/path/to/pico-sdk" >&2
    exit 1
fi

cmake -S "${project_dir}" -B "${build_dir}"
cmake --build "${build_dir}"

echo
echo "Built: ${build_dir}/PicoGhostHID.uf2"
