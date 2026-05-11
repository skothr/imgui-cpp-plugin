# 07 — Docking editor layout

IDE-style Dear ImGui shell for a game-engine editor. Targets the docking branch
(v1.92.7-docking) with GLFW + OpenGL 3 on Linux, C++23.

## What it does

- Fullscreen dockspace filling the main viewport, with a `File / Edit / View / Help`
  menu bar across the top (items are placeholders).
- Three default panels docked on first run via `DockBuilder`:
  - **Hierarchy** on the left (`scene graph goes here`)
  - **Viewport** in the center (`render target goes here`)
  - **Inspector** on the right (`selection details go here`)
- First-run layout is built programmatically (`build_default_layout`), so users get
  a sensible default with no `imgui.ini` present. On subsequent runs ImGui restores
  the user-adjusted layout from `imgui.ini`, and the default-layout pass becomes a
  no-op (it checks `DockBuilderGetNode(...)->IsSplitNode()` first).
- Multi-viewport is enabled (`ImGuiConfigFlags_ViewportsEnable`), so panels detach
  into native OS windows when dragged outside the host frame and can be parked on
  another monitor.

## Build / run

```bash
mkdir build && cd build
cmake ..
cmake --build .
./main
```

CMake fetches Dear ImGui v1.92.7-docking and GLFW 3.4 via `FetchContent`; system
OpenGL is the only external dependency.

## Files

- `CMakeLists.txt` — FetchContent for ImGui + GLFW, OpenGL3 backend, ImGui headers
  marked `SYSTEM` so strict warnings in `main`'s code don't flood from ImGui internals.
- `src/main.cpp` — init, dockspace host with menu bar, first-run layout, the three
  panels, render + multi-viewport tail.
- `src/imscoped.hpp` — RAII scope guards for paired `Begin`/`End` and `Push`/`Pop` calls.

## Wiping the saved layout

Delete `build/imgui.ini` (or wherever you ran the binary from) and relaunch to get
the default Hierarchy/Viewport/Inspector split back.
