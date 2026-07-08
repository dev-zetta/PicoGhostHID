#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
deps_dir="${project_dir}/deps"
sdk_dir="${deps_dir}/pico-sdk"
sdk_version="${PICO_SDK_VERSION:-2.3.0}"
sdk_repo="${PICO_SDK_REPO:-https://github.com/raspberrypi/pico-sdk.git}"

mkdir -p "${deps_dir}"

if [[ -d "${sdk_dir}/.git" ]]; then
    echo "Updating existing SDK checkout: ${sdk_dir}"
    git -C "${sdk_dir}" fetch --tags origin
    git -C "${sdk_dir}" checkout "${sdk_version}"
else
    echo "Cloning Raspberry Pi Pico SDK ${sdk_version} into ${sdk_dir}"
    git clone --branch "${sdk_version}" "${sdk_repo}" "${sdk_dir}"
fi

git -C "${sdk_dir}" submodule update --init --recursive

echo
echo "Pico SDK is ready:"
echo "  ${sdk_dir}"
echo
echo "Use it with:"
echo "  export PICO_SDK_PATH=${sdk_dir}"
echo "  ./scripts/build.sh"
