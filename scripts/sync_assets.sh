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
    "$destination/shadows/legs" \
    "$destination/shadows/rifle"

for index in 1 2 3 4 5 6 7; do
    cp "$soldier_source/legs/legs${index}.png" "$destination/legs/legs${index}.png"
    cp "$shadow_source/legs${index}.png" "$destination/shadows/legs/legs${index}.png"
done
for index in 1 2 3 4 5 6 7 8 9; do
    cp "$soldier_source/rifle/rifle${index}.png" "$destination/rifle/rifle${index}.png"
    cp "$shadow_source/rifle${index}.png" "$destination/shadows/rifle/rifle${index}.png"
done

echo "Synced 32 Phase 2 PNG files to: $destination"
