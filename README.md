# siege

`siege` is a native top-down C++ strategy game in early development. Milestone 1,
Phase 1 establishes the world model, fixed-rate simulation loop, resizable window,
and a five-zone battlefield. Troops, assets, combat, and player commands are not
part of this phase.

## Technology

- C++23
- SDL 3.4.10 (found locally when available, otherwise fetched by CMake)
- CMake 3.24 or newer

The source is written to remain portable between macOS and Windows. SDL is pinned
to a stable release for reproducible fallback builds; using an existing SDL3
package avoids a download when one is already installed.

## Build and run on macOS

Prerequisites are the Xcode Command Line Tools, CMake 3.24+, Git, and an internet
connection for the first configure if SDL3 is not installed. Homebrew SDL3 is
optional (`brew install sdl3`).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/siege
```

Pass `-DSIEGE_FETCH_SDL3=OFF` while configuring to require a system SDL3 package.

## Phase 1 behavior

- Close the window or press Escape to quit.
- The complete battlefield remains visible while the window is resized.
- World resolution: 1920x1080.
- Simulation rate: 60 fixed ticks per second, independent of rendering rate.

The purchased source sprite pack remains outside this repository and untouched.
Only the required assets will be copied into `assets/` during Phase 2.
