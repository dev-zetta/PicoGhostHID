#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
board="${1:-}"

case "${board}" in
    pico1)
        build_name="pico1"
        volume_label="RPI-RP2"
        ;;
    pico2)
        build_name="pico2"
        volume_label="RP2350"
        ;;
    rp2040zero)
        build_name="rp2040zero"
        volume_label="RPI-RP2"
        ;;
    hbb)
        build_name="hbb"
        volume_label="RPI-RP2"
        ;;
    *)
        echo "usage: ${0} <pico1|pico2|rp2040zero|hbb>" >&2
        exit 2
        ;;
esac

uf2_path="${project_dir}/build/${build_name}/PicoGhostHID.uf2"

if [[ ! -f "${uf2_path}" ]]; then
    echo "error: ${uf2_path} does not exist." >&2
    echo "Build it first:" >&2
    echo "  ./scripts/build_${build_name}.sh" >&2
    exit 1
fi

mapfile -t mount_points < <(
    lsblk -rno LABEL,MOUNTPOINTS |
        awk -v label="${volume_label}" '$1 == label && $2 != "" {print $2}'
)

if [[ "${#mount_points[@]}" -eq 0 ]]; then
    echo "error: ${volume_label} BOOTSEL volume is not mounted." >&2
    echo "Hold BOOTSEL while plugging in the Pico, then rerun:" >&2
    echo "  ./scripts/upload_${build_name}.sh" >&2
    exit 1
fi

if [[ "${#mount_points[@]}" -gt 1 ]]; then
    echo "error: multiple ${volume_label} mount points found:" >&2
    printf '  %s\n' "${mount_points[@]}" >&2
    exit 1
fi

mount_point="${mount_points[0]}"
target_path="${mount_point}/PicoGhostHID.uf2"

echo "Uploading ${uf2_path} to ${target_path}"
cp "${uf2_path}" "${target_path}"
sync
echo "Upload complete. The Pico should reboot out of BOOTSEL mode."
