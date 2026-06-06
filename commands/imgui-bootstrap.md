---
description: Scaffold a new Dear ImGui + imtool-toolkit project (CMake + C++23). Accepts a graphics backend; ships an Application-wired app (window + NodeGraph + KeyBindings), not a bare demo window.
argument-hint: "[backend] [target-directory]   # backend: opengl3(default)|vulkan|dx11|dx12|metal|sdl2-renderer|sdl3-renderer|wgpu"
---

Bootstrap a fresh Dear ImGui project wired to the **imtool toolkit** at the given path (default: current directory).

## Backend

Parse a graphics backend from `$ARGUMENTS` (first token if it matches the list below); if absent, ask the user. Default: **opengl3**.

- `opengl3` — **fully supported, end-to-end.** GLFW + OpenGL3, the toolkit's `imtool::Application`.
- `vulkan` | `dx11` | `dx12` | `metal` | `sdl2-renderer` | `sdl3-renderer` | `wgpu` — **roadmap.** No backend-specific scaffold yet. Tell the user verbatim: `backend <X> is roadmap — falling back to opengl3` and scaffold opengl3. These map to the backend feature requests (Linear MAIN-2…MAIN-7) and the bootstrap-backend tracking issue; mention that the toolkit `Application` is currently GLFW+OpenGL3-only and other backends need its platform/renderer layer generalized.

Do **not** silently substitute — always print the fallback line so the user knows they got opengl3.

## Steps

1. Invoke the `imgui-cpp-development` skill so its conventions and routing load.
2. Load `skills/imgui-cpp-development/references/bootstrap.md` for the full procedure (init order, shutdown order, the traps the templates already handle).
3. Resolve the target directory. If non-empty, list its contents and ask whether to scaffold inside (creating subdirs) or pick another path.
4. Resolve the backend (above).
5. **opengl3 (default) — toolkit-wired scaffold:**
   - Copy `assets/CMakeLists-imtool-glfw-opengl3.txt.template` → `<target>/CMakeLists.txt`.
   - Copy `assets/main_imtool_glfw_opengl3.cpp.template` → `<target>/src/main.cpp`.
   - The CMakeLists fetches Dear ImGui (core + GLFW + OpenGL3 backends), GLFW, nlohmann/json, and the **imtool toolkit** (`toolkit/` of this plugin repo, `IMTOOL_BUILD_APP=ON`), then links `imtool::app` into `main`. `main.cpp` subclasses `imtool::Application` and shows a live `NodeGraph` view + a `KeyBindingManager` — a running toolkit app, not a bare demo window.
   - Pinning: the toolkit is fetched at `IMTOOL_GIT_TAG` (default `main`). Tell the user to pin it to a released tag (e.g. `v0.1.0`) for reproducibility, and that `-DFETCHCONTENT_SOURCE_DIR_IMTOOL=/path/to/imgui-cpp-plugin` builds against a local checkout.
   - Print the next-steps block: `mkdir build && cd build && cmake .. && cmake --build .`
6. **`--minimal` flag (optional):** if the user wants a bare ImGui app with **no toolkit dependency**, use the older `assets/CMakeLists-glfw-opengl3.txt.template` + `assets/main_glfw_opengl3.cpp.template` + `assets/imscoped.hpp` instead. This is the plain GLFW+OpenGL3 starter (a demo window), useful for learning the raw frame loop.
7. Confirm the resulting tree with the user before declaring done.

## Conventions applied automatically

- C++23 (set in the CMake template).
- `imtool::Application` owns the GLFW window + GL context + ImGui context + frame loop (DPI scaling, optional docking/viewports via `AppConfig`); the toolkit headers are marked `SYSTEM` so the app's strict warnings don't fire on them.
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` so clangd / LSP works.
- For `--minimal`: `imscoped.hpp` scope guards, docking + multi-viewport in the IO config, per-monitor DPI via `ConfigDpiScaleFonts`.

## Boundary

This plugin owns the build + code templates. A wrapper (e.g. claude-config's `init_project.py --style cpp-imgui`) only writes the Claude overlay and then points the user here; it does not generate build/source files.

If anything fails (target dir missing, FetchContent blocked by no network on first build, a roadmap backend requested), report the failure with the recovery path. Don't auto-retry blindly.
