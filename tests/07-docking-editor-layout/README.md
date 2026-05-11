# Docking editor layout

IDE-style scaffold for a game-engine editor built on Dear ImGui v1.92.7-docking
(GLFW + OpenGL 3, C++23).

## Layout

```
+-----------+-----------------------+-----------+
| Hierarchy |       Viewport        | Inspector |
|  (left)   |       (center)        |  (right)  |
+-----------+-----------------------+-----------+
```

- Fullscreen dockspace fills the main viewport.
- Top menu bar: `File / Edit / View / Help` (menu items are placeholders, no-op).
- Three default panels are docked on first run via `DockBuilder`.
- Multi-viewport is on: drag any panel out of the host window and it becomes a
  native OS window. Drop it back onto a dock target to re-dock.
- Layout persists in `imgui.ini` next to the binary. Delete that file to reset
  to the first-run default.

## Build & run

Requires CMake >= 3.21 and a C++23 compiler. CMake `FetchContent` pulls
Dear ImGui v1.92.7-docking and GLFW 3.4 — no system packages needed beyond
OpenGL development headers.

```sh
cmake -S . -B build
cmake --build build -j
./build/main
```

On Linux you'll also need OpenGL/X11 dev packages (e.g. on Debian/Ubuntu:
`sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`).

## Files

- `CMakeLists.txt` — FetchContent for ImGui + GLFW, builds ImGui as a static lib.
- `src/main.cpp` — entry point: GLFW + OpenGL3 init, dockspace + menu bar, three panels.
- `src/imscoped.hpp` — RAII scope guards for ImGui's paired Begin/End and Push/Pop calls.

## Notes on first-run layout

`submit_editor_dockspace()` checks `DockBuilderGetNode(dock_id) == nullptr` and
only seeds the default split when no node exists yet. Once ImGui has loaded
`imgui.ini` (automatic on first `NewFrame()`), the node exists, so DockBuilder
is skipped and the user's adjusted layout is preserved across runs.
