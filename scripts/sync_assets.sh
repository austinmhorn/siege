#!/usr/bin/env bash

set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_root="${1:-$project_root/.local_assets/soldiers}"
font_source_root="${2:-$project_root/.local_assets/fonts/dogica}"
terrain_source_root="${3:-$project_root/.local_assets/terrain/tilesets}"
pack_root="${source_root%/}/PNG"
soldier_source="$pack_root/soldiers_color2/soldier1"
shadow_source="$pack_root/Shadows"
destination="$project_root/assets/soldiers/color2/soldier1"
shadow_destination="$project_root/assets/soldiers/shared/shadows"
font_destination="$project_root/assets/fonts/dogica"
terrain_destination="$project_root/assets/terrain/battlefield"

required_files=()
for index in 1 2 3 4 5 6 7; do
    required_files+=(
        "$soldier_source/legs/legs${index}.png"
        "$shadow_source/legs${index}.png"
    )
done
required_font_files=(
    "$font_source_root/TTF/dogicapixel.ttf"
    "$font_source_root/TTF/dogicapixelbold.ttf"
    "$font_source_root/dogica_pixel_license.txt"
)
terrain_source_files=(
    "$terrain_source_root/PNG/Tileset_v2/Tiles/Grass/tile_0029_grass6.png"
    "$terrain_source_root/PNG/Tileset_v2/Tiles/Dirt/tile_0005_dirt6.png"
    "$terrain_source_root/PNG/Tileset_v2/Tiles/Asphalt/tile_0108_asphalt7.png"
    "$terrain_source_root/PNG/Tileset_v2/Tiles/Sand/tile_0055_sand7.png"
    "$terrain_source_root/PNG/Tileset_v2/Tiles/Water/tile_0077_water4.png"
    "$terrain_source_root/PNG/House/TDS04_0000_House01.png"
    "$terrain_source_root/PNG/Rocks/TDS04_0004_Rock02.png"
    "$terrain_source_root/PNG/SandBag/TDS04_0002_Sandbags.png"
    "$terrain_source_root/PNG/Trees Bushes/TDS04_0021_Tree2.png"
    "$terrain_source_root/PNG/Trees Bushes/TDS04_0011_Bush-02.png"
    "$terrain_source_root/PNG/WatchTower/TDS04_0009_WatchTower.png"
    "$terrain_source_root/PNG/Crates Barrels/TDS04_0013_Box-02.png"
    "$terrain_source_root/PNG/Crates Barrels/TDS04_0016_Barrel.png"
    "$terrain_source_root/PNG/Shadows/TDS04_0000_House01.png"
    "$terrain_source_root/PNG/Shadows/TDS04_0004_Rock02.png"
    "$terrain_source_root/PNG/Shadows/TDS04_0002_Sandbags.png"
    "$terrain_source_root/PNG/Shadows/TDS04_0021_Tree2.png"
    "$terrain_source_root/PNG/Shadows/TDS04_0011_Bush-02.png"
    "$terrain_source_root/PNG/Shadows/TDS04_0009_WatchTower.png"
    "$terrain_source_root/PNG/Shadows/TDS04_0013_Box-02.png"
    "$terrain_source_root/PNG/Shadows/TDS04_0016_Barrel.png"
)
terrain_runtime_files=(
    "tiles/grass.png"
    "tiles/dirt.png"
    "tiles/asphalt.png"
    "tiles/sand.png"
    "tiles/water.png"
    "objects/house_01.png"
    "objects/rock_02.png"
    "objects/sandbags_01.png"
    "objects/tree_02.png"
    "objects/bush_02.png"
    "objects/watchtower_01.png"
    "objects/crate_02.png"
    "objects/barrel_01.png"
    "shadows/house_01.png"
    "shadows/rock_02.png"
    "shadows/sandbags_01.png"
    "shadows/tree_02.png"
    "shadows/bush_02.png"
    "shadows/watchtower_01.png"
    "shadows/crate_02.png"
    "shadows/barrel_01.png"
)
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
for file in "${required_font_files[@]}"; do
    if [[ ! -f "$file" ]]; then
        echo "error: required Dogica Pixel asset not found: $file" >&2
        exit 1
    fi
done
if [[ ! -d "$terrain_source_root" ]]; then
    echo "error: terrain source pack not found: $terrain_source_root" >&2
    exit 1
fi
for file in "${terrain_source_files[@]}"; do
    if [[ ! -f "$file" ]]; then
        echo "error: required terrain asset not found: $file" >&2
        exit 1
    fi
done

mkdir -p \
    "$destination/legs" \
    "$destination/rifle" \
    "$destination/machine_gun" \
    "$destination/bazooka" \
    "$destination/death1" \
    "$shadow_destination/legs" \
    "$shadow_destination/rifle" \
    "$shadow_destination/machine_gun" \
    "$shadow_destination/bazooka" \
    "$shadow_destination/death1"
mkdir -p "$font_destination"
mkdir -p \
    "$terrain_destination/tiles" \
    "$terrain_destination/objects" \
    "$terrain_destination/shadows"

for index in 1 2 3 4 5 6 7; do
    cp "$soldier_source/legs/legs${index}.png" "$destination/legs/legs${index}.png"
    cp "$shadow_source/legs${index}.png" "$shadow_destination/legs/legs${index}.png"
done
for index in 1 2 3 4 5 6 7 8 9; do
    cp "$soldier_source/rifle/rifle${index}.png" "$destination/rifle/rifle${index}.png"
    cp "$shadow_source/rifle${index}.png" "$shadow_destination/rifle/rifle${index}.png"
done
for index in 1 2 3 4; do
    cp "$soldier_source/death1/death1_${index}.png" \
        "$destination/death1/death1_${index}.png"
    cp "$shadow_source/death1_${index}.png" \
        "$shadow_destination/death1/death1_${index}.png"
done
for index in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
    cp "$soldier_source/machine_gun/machine_gun${index}.png" \
        "$destination/machine_gun/machine_gun${index}.png"
    cp "$shadow_source/machine_gun${index}.png" \
        "$shadow_destination/machine_gun/machine_gun${index}.png"
done
for index in 1 2 3 4 5 6 7 8 9 10 11 12 13; do
    cp "$soldier_source/bazooka/bazooka${index}.png" \
        "$destination/bazooka/bazooka${index}.png"
    cp "$shadow_source/bazooka${index}.png" \
        "$shadow_destination/bazooka/bazooka${index}.png"
done

cp "$font_source_root/TTF/dogicapixel.ttf" \
    "$font_destination/dogicapixel.ttf"
cp "$font_source_root/TTF/dogicapixelbold.ttf" \
    "$font_destination/dogicapixelbold.ttf"
cp "$font_source_root/dogica_pixel_license.txt" \
    "$font_destination/LICENSE.txt"

for index in "${!terrain_source_files[@]}"; do
    cp "${terrain_source_files[$index]}" \
        "$terrain_destination/${terrain_runtime_files[$index]}"
done

echo "Synced authored blue soldier PNG files to: $destination"
echo "Synced shared shadow PNG files to: $shadow_destination"
echo "Synced Dogica Pixel regular, bold, and license files to: $font_destination"
echo "Synced battlefield_01 terrain and environment PNG files to: $terrain_destination"
