I want an IDE-style Dear ImGui layout for a game-engine editor I'm starting. Targeting the docking branch (v1.92.x). The intended look:

- A fullscreen dockspace filling the main viewport.
- A top menu bar (`File / Edit / View / Help` is fine, items can be no-ops for now — placeholders, no actual logic).
- Three default panels docked on first run:
  - Left: `Hierarchy` (placeholder text "scene graph goes here").
  - Center: `Viewport` (placeholder text "render target goes here", filling its dock area).
  - Right: `Inspector` (placeholder text "selection details go here").
- The first-run layout should set itself up automatically so users get a sensible default. Subsequent runs should respect any user-adjusted layout.
- Panels should be detachable into native OS windows when the user drags them out.

GLFW + OpenGL3 backend, C++23, Linux. Output the full project under `tests/07-docking-editor-layout/` with a README that includes the build/run commands. No need to actually compile.
