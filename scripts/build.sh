#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
build_dir="${project_dir}/build"
local_sdk_dir="${project_dir}/deps/pico-sdk"

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

cmake -S "${project_dir}" -B "${build_dir}"
cmake --build "${build_dir}"

echo
echo "Built: ${build_dir}/PicoGhostHID.uf2"
