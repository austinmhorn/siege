# siege

`siege` is a native top-down C++ strategy game in early development. Milestone 4
now includes mixed rifle, machine-gun, and bazooka combat built on shared fixed-step
targeting, projectile, damage, death-lifecycle, and client-effects systems.

## Technology

- C++23
- SDL 3.4.10 (including its built-in PNG decoder; found locally when available,
  otherwise fetched by CMake)
- SDL3_ttf 3.2.2 for client-side TrueType rendering (found locally when
  available, otherwise fetched by CMake)
- Dogica Pixel by Roberto Mocci for in-game typography
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

The script defaults to `.local_assets/soldiers/` and
`.local_assets/fonts/dogica/` at the repository root. The first directory
contains the developer's private purchased CraftPix source pack; the second
contains the local Dogica Pixel distribution. Pass alternate soldier and font
roots as the first and second arguments when needed. The script copies only the
required `soldiers_color1/soldier1` layers and shadows into `assets/soldiers/`,
plus `dogicapixel.ttf`, `dogicapixelbold.ttf`, and the SIL Open Font License into
`assets/fonts/dogica/`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/siege
```

Pass `-DSIEGE_FETCH_SDL3=OFF` or `-DSIEGE_FETCH_SDL3_TTF=OFF` while configuring
to require the corresponding system package.

GitHub Actions configures, builds, and runs CTest on `macos-latest` and
`windows-latest` for pushes and pull requests involving `dev` or `main`. CI does
not require or sync proprietary artwork; asset copying is enabled automatically
for local builds when the generated `assets/` directory exists.

All current game and F3 text uses the client-side Dogica Pixel renderer. Regular
and bold faces are opened at centralized 8, 10, and 12 pixel roles, rendered as
solid glyph surfaces, cached as SDL textures, and drawn at integer coordinates
with nearest-neighbor scaling. If the runtime font files are absent, the game
logs the missing path and falls back to SDL's built-in debug font instead of
failing startup. Dogica Pixel is Copyright (c) 2020 Roberto Mocci and is used
under the SIL Open Font License 1.1; the sync step places that license beside the
runtime fonts. Tracked copies of the attribution and exact license text are in
`THIRD_PARTY_NOTICES.md` and `LICENSES/Dogica-Pixel-OFL-1.1.txt`.

## Current behavior

- Close the window or press Escape to quit.
- Press F3 to toggle per-unit simulation diagnostics. D is a temporary fallback
  for macOS keyboards that reserve F3 for Mission Control.
- Each team fields two rifles, two machine guns, and two bazookas. Team A advances
  right; Team B advances left.
- Moving units animate their legs independently. Actual shots briefly play the
  troop's matching upper-body firing sequence, then return to its non-firing frame.
- Rifle perception uses a 500-unit, 90-degree forward cone and a 110-unit
  omnidirectional awareness radius.
- The F3 overlay draws the current-facing vision cone and awareness radius.
- Units retain a perceptible target by ID, reacquire the nearest perceptible enemy
  after losing it, and gradually turn toward the selected target.
- The F3 overlay labels each unit's target and draws a red observer-to-target line.
- Targeted rifle units close when farther than their `280 +/- 35` range band, hold
  while inside it, and retreat when too close; separation remains active. Their
  balanced baseline uses a 0.90 pursuit factor, 0.75 retreat factor, and no
  support-positioning bias.
- The F3 overlay reports combat movement state and draws preferred combat range.
- Data-driven behavior profiles control pursuit, retreat, frontline eligibility,
  and support positioning. Nearby rifle screens pull support troops toward a
  troop-specific rear offset; no formal squad state is created, and close-range
  retreat takes priority over that influence.
- Aligned rifle units fire 960-unit/second tracers every 0.60 seconds while their
  target is inside the 360-unit weapon range and 12-degree firing arc. Rifle
  projectiles deal 25 damage on the first swept-circle hit against a hostile
  unit; rifles have 100 health and a 20-unit hit radius. Units at zero health stop
  participating immediately, emit one death event, and leave active World units.
- Machine guns move at 54 units/second, rotate at 60 degrees/second, and prefer a
  `390 +/- 45` engagement band. They fire 1100-unit/second projectiles every 0.18
  seconds within a 480-unit range and 14-degree arc, dealing 10 damage per hit.
  Their pursuit factor is 0.62 and retreat factor is 0.85. A 0.85 support bias
  seeks a position 120 units behind a rifle within 420 units. Their longer
  600-unit vision and sustained cadence distinguish them from rifles; both troop
  definitions retain a future zone-control weight of 1.
- Bazookas move at 60 units/second, rotate at 50 degrees/second, and prefer a
  `520 +/- 55` engagement band. Their 480-unit/second rockets fire every 2.60
  seconds within a 650-unit range and 10-degree arc. A hostile impact deals 70
  damage once to every hostile unit whose center is within the 115-unit splash
  radius; the source and all friendlies remain immune. Their pursuit factor is
  0.50 and retreat factor is 1.0. A 1.10 support bias seeks a position 180 units
  behind a rifle within 500 units. Bazookas have 80 health and retain a future
  zone-control weight of 1.
- Death events create a client-only, non-looping `death1` animation. Its final
  corpse frame then fades smoothly for 10 seconds before being destroyed.
- Projectiles travel independently according to their weapon profile. F3 reports
  active unit, projectile, corpse, firing-effect, explosion-effect, health, and
  weapon-cooldown state. It also labels behavior factors and the selected friendly
  screen, with a short green line for active support steering.
- Objective zones 1-3 count living Team A and Team B troops at one control point
  each. Their persistent `-100` (Team B) to `+100` (Team A) meters move at 5 points
  per second for each net troop, with effective pressure capped at 3. Empty and
  equally contested objectives retain their current progress; home zones do not
  participate. Neutral objectives become owned only at either full extreme. An
  owned objective remains owned while its meter stays on that team's side of zero,
  becomes neutral at zero, and must then reach the opposite extreme before enemy
  capture. Transient neutralization and capture events report every actual owner
  transition once. F3 shows owner, counts, raw pressure, capture value, and a
  directional bar; owned objectives use subtle team tints outside debug mode.
- An owned objective reports occupied when its owner has living troops inside and
  contested whenever enemy troops are present. It becomes secured after two
  continuous enemy-free seconds; enemy entry immediately resets that state and
  timer. Home zones are permanently secured. A secured objective is deployable
  only by its owner, within the team-relative rear 75 percent of its bounds. F3
  reports these states and outlines the valid deployment region; actual deployment,
  economy, and scoring are not implemented yet.
- The complete battlefield remains visible while the window is resized.
- World resolution: 1920x1080.
- Simulation rate: 60 fixed ticks per second, independent of rendering rate.
- Team A and Team B each begin with 25,000 integer cash and receive identical
  passive income of 100 cash per second. Income uses an integer fixed-tick
  remainder, so simulation time—not rendering rate—controls cash. F3 displays
  both balances and the passive rate; purchasing and rewards are not implemented.

The private source inputs under `.local_assets/` and generated runtime content
under `assets/soldiers/` and `assets/fonts/` are intentionally excluded from
Git. The sync script only reads source inputs, and local builds copy the runtime
subset beside the executable. Development happens on `dev`; reviewed stable work
is promoted to `main` by the repository owner.
