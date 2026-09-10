# siege

`siege` is a native top-down C++ strategy game in early development. Milestone 1,
Phase 2 adds the local asset pipeline and one stationary animated CraftPix `rifle`
soldier to the Phase 1 battlefield. Movement, combat, AI, and player commands are
not part of this phase.

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
pack root as its first argument when needed. It copies only the Phase 2
`soldiers_color1/soldier1` leg and rifle layers plus their matching shadows.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/siege
```

Pass `-DSIEGE_FETCH_SDL3=OFF` while configuring to require a system SDL3 package.

## Phase 1 behavior

- Close the window or press Escape to quit.
- Press R to rotate the demonstration soldier by an arbitrary test increment.
- The complete battlefield remains visible while the window is resized.
- World resolution: 1920x1080.
- Simulation rate: 60 fixed ticks per second, independent of rendering rate.

Purchased artwork under `assets/` is intentionally ignored by Git and is copied
beside the executable during builds. The source pack remains outside this
repository and untouched. Development happens on `dev`; reviewed stable work is
promoted to `main` by the repository owner.
