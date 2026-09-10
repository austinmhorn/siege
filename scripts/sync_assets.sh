#!/usr/bin/env bash

set -euo pipefail

source_root="${1:-/Users/austinhorn/Downloads/top-down-soldier-sprites-pixel-art/}"
pack_root="${source_root%/}/PNG"
soldier_source="$pack_root/soldiers_color1/soldier1"
shadow_source="$pack_root/Shadows"
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
destination="$project_root/assets/soldiers/color1/soldier1"

required_files=()
for index in 1 2 3 4 5 6 7; do
    required_files+=(
        "$soldier_source/legs/legs${index}.png"
        "$shadow_source/legs${index}.png"
    )
done
for index in 1 2 3 4 5 6 7 8 9; do
    required_files+=(
        "$soldier_source/rifle/rifle${index}.png"
        "$shadow_source/rifle${index}.png"
    )
done
for index in 1 2 3 4; do
    required_files+=(
        "$soldier_source/death1/death1_${index}.png"
        "$shadow_source/death1_${index}.png"
    )
done
for index in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
    required_files+=(
        "$soldier_source/machine_gun/machine_gun${index}.png"
        "$shadow_source/machine_gun${index}.png"
    )
done
for index in 1 2 3 4 5 6 7 8 9 10 11 12 13; do
    required_files+=(
        "$soldier_source/bazooka/bazooka${index}.png"
        "$shadow_source/bazooka${index}.png"
    )
done

if [[ ! -d "$source_root" ]]; then
    echo "error: CraftPix source pack not found: $source_root" >&2
    exit 1
fi

for file in "${required_files[@]}"; do
    if [[ ! -f "$file" ]]; then
        echo "error: required source asset not found: $file" >&2
        exit 1
    fi
done

mkdir -p \
    "$destination/legs" \
    "$destination/rifle" \
    "$destination/machine_gun" \
    "$destination/bazooka" \
    "$destination/death1" \
    "$destination/shadows/legs" \
    "$destination/shadows/rifle" \
    "$destination/shadows/machine_gun" \
    "$destination/shadows/bazooka" \
    "$destination/shadows/death1"

for index in 1 2 3 4 5 6 7; do
    cp "$soldier_source/legs/legs${index}.png" "$destination/legs/legs${index}.png"
    cp "$shadow_source/legs${index}.png" "$destination/shadows/legs/legs${index}.png"
done
for index in 1 2 3 4 5 6 7 8 9; do
    cp "$soldier_source/rifle/rifle${index}.png" "$destination/rifle/rifle${index}.png"
    cp "$shadow_source/rifle${index}.png" "$destination/shadows/rifle/rifle${index}.png"
done
for index in 1 2 3 4; do
    cp "$soldier_source/death1/death1_${index}.png" \
        "$destination/death1/death1_${index}.png"
    cp "$shadow_source/death1_${index}.png" \
        "$destination/shadows/death1/death1_${index}.png"
done
for index in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
    cp "$soldier_source/machine_gun/machine_gun${index}.png" \
        "$destination/machine_gun/machine_gun${index}.png"
    cp "$shadow_source/machine_gun${index}.png" \
        "$destination/shadows/machine_gun/machine_gun${index}.png"
done
for index in 1 2 3 4 5 6 7 8 9 10 11 12 13; do
    cp "$soldier_source/bazooka/bazooka${index}.png" \
        "$destination/bazooka/bazooka${index}.png"
    cp "$shadow_source/bazooka${index}.png" \
        "$destination/shadows/bazooka/bazooka${index}.png"
done

echo "Synced 98 rifle, machine-gun, and bazooka soldier PNG files to: $destination"
