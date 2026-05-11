# 02-tools-panel

A small Dear ImGui (v1.92.7-docking) tools panel for prototyping. Single
non-docking window with Camera and Render groups, plus an Apply button that
prints the current state to stdout.

## Layout

```
02-tools-panel/
  CMakeLists.txt
  src/
    main.cpp
    imscoped.hpp
  README.md
```

Dear ImGui and GLFW are pulled in by CMake's `FetchContent` at configure time;
no system packages required beyond a C++23 compiler, OpenGL, and the usual
X11/Wayland dev headers GLFW needs.

## Prerequisites (Linux)

- `clang` >= 16 (for C++23 `<print>`) or `g++` >= 14
- `cmake` >= 3.21
- OpenGL development headers (`libgl1-mesa-dev` on Debian/Ubuntu)
- X11 dev headers GLFW depends on:
  `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`
  (or the Wayland equivalents if you prefer)

## Build

```sh
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j
```

## Run

```sh
./build/main
```

Adjust the controls and click **Apply** — the panel state prints to stdout,
one field per line.

## Controls

- **Camera**
  - `FOV (deg)` — slider, 30..120
  - `Near plane` — drag-float, clamped to `(0, far_plane]`
  - `Far plane` — drag-float, clamped to `[near_plane, 100000]`
  - `Reset camera` — restores 60deg / 0.1 / 1000
- **Render**
  - `Wireframe` — checkbox (state only; this prototype doesn't wire it into a
    real render path)
  - `Clear color` — drives `glClearColor` directly
  - `Demo text` — a line whose `ImGuiCol_Text` is overridden locally via an
    `ImScoped::StyleColor` guard, so you can see the style stack at work
  - `Demo text color` — color picker driving that override
- **Apply** — prints current state to stdout
