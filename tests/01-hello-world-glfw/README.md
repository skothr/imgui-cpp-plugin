# 01-hello-world-glfw

Minimal Dear ImGui (v1.92.7-docking) + GLFW 3.4 + OpenGL 3 starter. Renders the
ImGui demo window so you can browse available widgets.

CMake uses `FetchContent` to pull Dear ImGui and GLFW at configure time — no
system packages or git submodules required (just a working network connection
on the first configure).

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build . -j
```

First configure downloads Dear ImGui + GLFW (a few minutes). Subsequent builds
are incremental.

## Run

From the `build/` directory:

```bash
./main
```

You should see a window with the ImGui demo open. Toggle the demo / about /
metrics windows from the top-left controls in `src/main.cpp`.

## Layout

```
01-hello-world-glfw/
  CMakeLists.txt        FetchContent for imgui + glfw, builds imgui as static lib
  src/
    main.cpp            GLFW+OpenGL3 entry point, demo window enabled
    imscoped.hpp        RAII scope guards for paired Begin/End and Push/Pop
```

`imscoped.hpp` is bundled so paired ImGui calls (`Begin`/`End`,
`PushID`/`PopID`, etc.) can be written as scoped C++ objects rather than
hand-paired calls — see the existing usage in `main.cpp`.

## Requirements

- CMake >= 3.20
- A C++23-capable compiler (clang 17 / gcc 13+)
- Network access on the first `cmake ..` (for FetchContent)
- Linux: X11 or Wayland dev headers for GLFW (`libx11-dev libxrandr-dev
  libxinerama-dev libxcursor-dev libxi-dev` on Ubuntu, or the Wayland
  equivalents).
