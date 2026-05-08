I want an IDE-style Dear ImGui layout for a game-engine editor I'm starting. Targeting the docking branch (v1.92.x). The intended look:

- A fullscreen `DockSpace` filling the main viewport.
- A top menu bar (`File / Edit / View / Help` is fine, items can be no-ops for now — `MenuItem` placeholders, no actual logic).
- Three default panels docked into the dockspace on first run:
  - Left: `Hierarchy` (just renders a placeholder text "scene graph goes here").
  - Center: `Viewport` (placeholder text "render target goes here", filling its dock area).
  - Right: `Inspector` (placeholder text "selection details go here").
- The first-run layout should be built programmatically via `DockBuilder` so users get a sensible default. Subsequent runs read the saved layout from `.ini` and respect user adjustments.
- Multi-viewport (`ConfigFlags_ViewportsEnable`) on, so panels can be dragged out into native OS windows.

GLFW + OpenGL3 backend, C++23, Linux. Output the full project under `tests/07-docking-editor-layout/` (CMakeLists.txt that fetches imgui v1.92.7-docking + GLFW 3.4 via FetchContent, src/main.cpp, src/imscoped.hpp, README.md with build/run instructions). I don't need you to actually compile.
