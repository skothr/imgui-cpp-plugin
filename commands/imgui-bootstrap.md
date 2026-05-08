---
description: Scaffold a new Dear ImGui project (CMake + GLFW + OpenGL3, C++23) from scratch in the current directory or a target path.
argument-hint: "[target-directory]"
---

Bootstrap a fresh Dear ImGui project at the given path (default: current directory).

Steps:

1. Invoke the `imgui-cpp-development` skill so its conventions and routing are loaded.
2. Load `skills/imgui-cpp-development/references/bootstrap.md` for the full procedure.
3. Confirm the target directory with the user. If non-empty, list its contents and ask whether to scaffold inside (creating subdirs) or pick a different path.
4. Ask the user which build system they want. CMake + FetchContent is the first-class default. For Meson / Bazel / Premake / raw Makefile, fall back to a generic flow and tell the user that deeper scaffolding for those is on the roadmap (Linear feature requests MAIN-8 through MAIN-11).
5. For the CMake path:
   - Copy `assets/CMakeLists-glfw-opengl3.txt.template` → `<target>/CMakeLists.txt`.
   - Copy `assets/main_glfw_opengl3.cpp.template` → `<target>/src/main.cpp`.
   - Copy `assets/imscoped.hpp` → `<target>/src/imscoped.hpp`.
   - Print the next-steps block: `mkdir build && cd build && cmake .. && cmake --build .`
6. Confirm the resulting tree with the user before declaring done.

Conventions to apply automatically:
- C++23 standard (already set in the CMake template).
- `imscoped.hpp` for paired Begin/End and Push/Pop calls (the bundled main.cpp template demonstrates this).
- Docking + multi-viewport enabled in the IO config.
- Per-monitor DPI scaling wired through `ConfigDpiScaleFonts`.
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` so clangd / LSP works.

If anything fails (the target dir doesn't exist, network blocks the FetchContent fetch on the user's first build, etc.), report the failure with the recovery path. Don't auto-retry blindly.
