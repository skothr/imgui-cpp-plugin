# 01-hello-world-glfw

Minimal Dear ImGui (v1.92.7-docking) hello-world using GLFW + OpenGL 3.

CMake fetches both Dear ImGui and GLFW via `FetchContent` — no system packages
or git submodules needed. The first configure does the network fetch; subsequent
builds are offline.

## Requirements

- CMake >= 3.21
- A C++23 compiler (clang 17 is fine)
- A system OpenGL library + headers (Ubuntu: `sudo apt install libgl1-mesa-dev`)
- X11 or Wayland dev headers for GLFW on Linux
  (Ubuntu: `sudo apt install xorg-dev` — GLFW's own README is the source of truth)

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build . -j
```

The first `cmake ..` clones `ocornut/imgui@v1.92.7-docking` and `glfw/glfw@3.4`
into `build/_deps/`. Allow a minute for that on the first run.

## Run

```bash
./main
```

You should get a 1280x800 window with the ImGui demo window already open, plus a
small "Hello, world!" panel showing FPS and a clear-color picker. Docking and
multi-viewport are both enabled, so you can drag the demo window out of the main
window onto your desktop.

## What's in here

- `CMakeLists.txt` — `FetchContent` for ImGui + GLFW, builds ImGui as a static
  lib, links into `main`. Emits `compile_commands.json` for clangd / LSP.
- `src/main.cpp` — full init / frame-loop / shutdown using `ImScoped::Window`.
- `src/imscoped.hpp` — header-only RAII guards for paired Begin/End and Push/Pop
  calls (Window, Child, Menu, Popup, Table, TabBar, ID, StyleColor, ...).

## Next steps

- Browse the demo window in-app to see widget examples. Each section in the demo
  maps to a block in upstream's `imgui_demo.cpp` — that file is the canonical
  "how do I X" reference.
- Replace the `ImScoped::Window("Hello, world!")` block in `src/main.cpp` with
  your own UI.
