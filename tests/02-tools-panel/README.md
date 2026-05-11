# 02-tools-panel

A small Dear ImGui tools panel scaffold for prototyping. Single window, no
docking. GLFW + OpenGL 3 backend, C++23, vendored ImGui (v1.92.7-docking) and
GLFW (3.4) fetched via CMake `FetchContent`.

## Layout

The window (`Tools`) contains, top to bottom:

- Header `"Prototyping Tools"` + separator.
- **Camera** group: FOV slider (degrees, 30..120), near/far plane drag-floats,
  `Reset camera` button (restores the compile-time defaults).
- **Render** group: `Wireframe` checkbox, clear-color picker (drives
  `glClearColor`), and a `Demo text` line whose color is overridden locally via
  an `ImScoped::StyleColor` guard so you can see the style stack pushing and
  popping around exactly one widget. A second color picker below it edits that
  override.
- `Apply` button: prints the current state to stdout, one field per line.

State lives in two plain structs (`CameraState`, `RenderState`) inside
`src/main.cpp` — extend them in place.

## Build & run (Linux, clang)

```sh
cd 02-tools-panel
cmake -S . -B build -G "Unix Makefiles" \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j
./build/main
```

First configure fetches ImGui and GLFW from GitHub (needs network). Subsequent
configures are cached in `build/_deps/`. `compile_commands.json` is emitted in
`build/` for clangd.

## Notes

- Strict warnings (`-Wall -Wextra -Wpedantic -Wold-style-cast -Wconversion
  -Wsign-conversion -Wdouble-promotion ...`) are on for `src/main.cpp` only;
  ImGui's headers are included as `SYSTEM` so its own old-style-cast / memset
  tricks don't drown out warnings in your code.
- Diagnostics use `std::fprintf` / `std::printf` rather than `std::println` —
  `<print>` requires libstdc++-14 or libc++-18+ and a clang new enough to
  compile `<bits/unicode.h>`. Switch in your own code once you've confirmed
  your toolchain.
- Begin/End and Push/Pop pairs use the RAII guards in `src/imscoped.hpp`
  (`ImScoped::Window`, `ImScoped::ID`, `ImScoped::StyleColor`).
