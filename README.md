# siege

`siege` is a native top-down C++ strategy game in early development. Milestone 4
now includes mixed rifle and machine-gun combat built on shared fixed-step targeting,
projectile, damage, death-lifecycle, and client-effects systems.

## Technology

- C++23
- SDL 3.4.10 (including its built-in PNG decoder; found locally when available,
  otherwise fetched by CMake)
- CMake 3.24 or newer

The source is written to remain portable between macOS and Windows. SDL is pinned
to a stable release for reproducible fallback builds; using an existing SDL3
package avoids a download when one is already installed.

## Build and run on macOS

Prerequisites are the Xcode Command Line Tools, CMake 3.24+, Git, and an internet
connection for the first configure if SDL3 is not installed. Homebrew SDL3 is
optional (`brew install sdl3`). Before building, sync the locally purchased art:

```sh
./scripts/sync_assets.sh
```

The script defaults to the original CraftPix pack at
`/Users/austinhorn/Downloads/top-down-soldier-sprites-pixel-art/`. Pass a different
pack root as its first argument when needed. It copies only the required
`soldiers_color1/soldier1` leg, rifle, machine-gun, and `death1` layers plus
matching shadows.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/siege
```

Pass `-DSIEGE_FETCH_SDL3=OFF` while configuring to require a system SDL3 package.

## Current behavior

- Close the window or press Escape to quit.
- Press F3 to toggle per-unit simulation diagnostics. D is a temporary fallback
  for macOS keyboards that reserve F3 for Mission Control.
- Each team fields two rifles and two machine guns. Team A advances right; Team B
  advances left.
- Moving units animate their legs independently. Actual shots briefly play the
  troop's matching upper-body firing sequence, then return to its non-firing frame.
- Rifle perception uses a 500-unit, 90-degree forward cone and a 110-unit
  omnidirectional awareness radius.
- The F3 overlay draws the current-facing vision cone and awareness radius.
- Units retain a perceptible target by ID, reacquire the nearest perceptible enemy
  after losing it, and gradually turn toward the selected target.
- The F3 overlay labels each unit's target and draws a red observer-to-target line.
- Targeted rifle units close when farther than their `280 +/- 35` range band, hold
  while inside it, and retreat when too close; separation remains active.
- The F3 overlay reports combat movement state and draws preferred combat range.
- Aligned rifle units fire 960-unit/second tracers every 0.60 seconds while their
  target is inside the 360-unit weapon range and 12-degree firing arc. Rifle
  projectiles deal 25 damage on the first swept-circle hit against a hostile
  unit; rifles have 100 health and a 20-unit hit radius. Units at zero health stop
  participating immediately, emit one death event, and leave active World units.
- Machine guns move at 54 units/second, rotate at 60 degrees/second, and prefer a
  `390 +/- 45` engagement band. They fire 1100-unit/second projectiles every 0.18
  seconds within a 480-unit range and 14-degree arc, dealing 10 damage per hit.
  Their longer 600-unit vision and sustained cadence distinguish them from rifles;
  both troop definitions retain a future zone-control weight of 1.
- Death events create a client-only, non-looping `death1` animation. Its final
  corpse frame then fades smoothly for 10 seconds before being destroyed.
- Projectiles travel independently for up to 520 world units. F3 reports active
  unit, projectile, corpse, firing-effect, health, and weapon-cooldown state.
- The complete battlefield remains visible while the window is resized.
- World resolution: 1920x1080.
- Simulation rate: 60 fixed ticks per second, independent of rendering rate.

Purchased artwork under `assets/` is intentionally ignored by Git and is copied
beside the executable during builds. The source pack remains outside this
repository and untouched. Development happens on `dev`; reviewed stable work is
promoted to `main` by the repository owner.
