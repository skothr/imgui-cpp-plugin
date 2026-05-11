# Bootstrap

> **Load this file when:** the user is setting up a new Dear ImGui project from scratch (build wiring, what to compile, what order to call init/shutdown in), or troubleshooting an app that crashes/asserts before the first frame ever renders.

This reference covers the minimum set of source files Dear ImGui v1.92.x needs, the headers consumers include, the canonical init/shutdown order, the recommended starter scaffolding (CMake + main), and the small handful of bootstrap mistakes upstream actively warns about. The per-frame work after `Init()` returns lives in [frame-loop.md](frame-loop.md). Detecting an existing ImGui copy (vs adding a new one) lives in [locate-imgui.md](locate-imgui.md).

## What you compile

Five core source files plus two backend source files for a GLFW + OpenGL3 app:

```
imgui.cpp
imgui_demo.cpp
imgui_draw.cpp
imgui_tables.cpp
imgui_widgets.cpp
backends/imgui_impl_glfw.cpp
backends/imgui_impl_opengl3.cpp
```

`imgui_demo.cpp` is optional in principle — drop it and `ImGui::ShowDemoWindow()` link-fails — but keep it. The demo is the canonical "how do I X" reference for both you and the model, and it costs nothing in release builds you don't call into. Headers come from the same directories: `imgui.h` (and friends it pulls in transitively) plus `backends/imgui_impl_glfw.h` and `backends/imgui_impl_opengl3.h`.

The OpenGL3 backend ships its own minimal GL loader (`imgui_impl_opengl3_loader.h`) so you do not need to link GLEW, glad, or glbinding. Don't set any `IMGUI_IMPL_OPENGL_LOADER_*` define unless an existing GL library in your project conflicts with the embedded loader.

## Headers a consumer includes

In a translation unit that submits ImGui UI:

```cpp
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "imscoped.hpp"  // RAII guards for paired Begin/End and Push/Pop calls

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>  // pulls in system OpenGL headers
```

GLFW pulls in GL automatically. Don't `#include <GL/gl.h>` directly unless you're calling GL functions outside the backend (e.g. your own `glClear`, `glViewport`, which the example does — those are picked up by the GLFW include chain).

## Init order — get this right or nothing renders

Reverse engineering this from a crash is painful. The canonical sequence (see `imgui.cpp:285-294` for the upstream sketch and `examples/example_glfw_opengl3/main.cpp:40-117` for the GLFW+GL3 specialization):

1. `glfwSetErrorCallback(...)` then `glfwInit()`. Set the error callback first so init errors actually surface.
2. `glfwWindowHint(...)` for GL version/profile, then `glfwCreateWindow(...)`, then `glfwMakeContextCurrent(window)`. The GL context has to be current before any backend init or GL call.
3. `IMGUI_CHECKVERSION();` — a one-line macro that asserts the header you compiled against matches the `imgui.cpp` you linked. Skipping this turns binary-incompatibility bugs (struct layout drift between header and lib) into mysterious crashes hours later.
4. `ImGui::CreateContext();` — must come before any other `ImGui::*` call. `ImGui::GetIO()` is only valid after this returns.
5. Configure `io` — `io.ConfigFlags`, DPI, font loading, etc. Anything that affects backend init has to be set here. In particular, `ImGuiConfigFlags_DockingEnable` and `ImGuiConfigFlags_ViewportsEnable` are read by the backends at `Init` time; flipping them mid-loop is unreliable (per `docs/BACKENDS.md`).
6. `ImGui::StyleColors{Dark,Light,Classic}();` and any `style.ScaleAllSizes(...)` for DPI.
7. Backend init — platform first, then renderer:
   ```cpp
   ImGui_ImplGlfw_InitForOpenGL(window, /*install_callbacks=*/true);
   ImGui_ImplOpenGL3_Init(glsl_version);  // pass nullptr to auto-detect
   ```
   `install_callbacks=true` lets ImGui hook GLFW's input callbacks and chain to any pre-existing handlers. Pass `false` only when you need to interpose your own logic before ImGui sees the event.

For multi-viewport, `ConfigFlags |= ImGuiConfigFlags_ViewportsEnable` has to be set before backend init, not after the first `NewFrame()`.

## Shutdown — strict reverse order

```cpp
ImGui_ImplOpenGL3_Shutdown();   // renderer first (it references context state)
ImGui_ImplGlfw_Shutdown();      // platform second
ImGui::DestroyContext();        // ImGui core last

glfwDestroyWindow(window);
glfwTerminate();
```

Backends destroy GL/GLFW resources that reference the live ImGui context, so they have to run before `DestroyContext()`. `DestroyContext()` is technically optional for a single-context app (the OS reclaims memory on exit), but call it — it makes leak detectors usable and is required if you ever add a second context.

## Recommended starting point

Don't retype either file from memory. Copy these two templates from the skill's assets:

- `assets/main_glfw_opengl3.cpp.template` — a complete C++23 main using `ImScoped::*` guards, docking + multi-viewport on, per-monitor DPI.
- `assets/CMakeLists-glfw-opengl3.txt.template` — CMake project that fetches ImGui v1.92.7-docking and GLFW 3.4 via `FetchContent` and emits `compile_commands.json` for clangd.

The CMake template's `FetchContent` block:

```cmake
include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.7-docking
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(imgui glfw)
```

ImGui then gets compiled as a static library (the 5 + 2 cpps above) and linked into your executable alongside `glfw` and `OpenGL::GL`. `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` is non-negotiable — without it, clangd / LSP cannot follow you into ImGui's source.

## Build-system hygiene — two traps the template already handles

Two patterns this skill's bundled template gets right that hand-written CMakeLists in the wild typically don't. Both produce real build failures or unreadable warning floods on the user's first compile. If you're regenerating a CMakeLists from scratch, replicate these.

### Mark vendored headers as `SYSTEM`

```cmake
target_include_directories(imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
```

The user's own target almost always enables a strict warning set — `-Wall -Wextra -Wpedantic -Wold-style-cast -Wconversion -Wsign-conversion -Wcast-align -Wdouble-promotion -Wformat=2` is a reasonable default. Those flags are great for the user's code. But ImGui's headers contain patterns that intentionally trip every one of them: `((float*)(void*)(char*)this)[idx]` in `ImVec2::operator[]`, `memset((void*)this, 0, sizeof(*this))` constructor zeroing, `(float)r * (1.0f/255.0f)` in `ImColor`, etc. Every `#include <imgui.h>` from a strict-flag translation unit produces 50+ lines of `-Wold-style-cast` warnings before any of the user's own code emits a thing.

`SYSTEM` translates per-compiler:
- GCC / Clang → `-isystem` (warnings silenced by default for these paths)
- MSVC → `/external:I` (same behavior)

The user's strict flags **remain fully in effect for their own .cpp files** — only the vendored headers get the pass. This is the canonical CMake idiom for "I'm vendoring a third-party library and I don't want its warning floor to bleed into mine."

### Default diagnostics to `std::fprintf`, not `std::println`

```cpp
#include <cstdio>
void glfw_error_callback(int e, const char* msg) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", e, msg);
}
```

`<print>` and the `<format>` it builds on require **libstdc++-14 or libc++-18+**, AND a compiler whose ranges-constraint checking is compatible with the chosen stdlib. Two failure shapes seen in practice on Ubuntu 22.04 + clang 17 (a toolchain users frequently name):

- libstdc++-11/12 (jammy default): `fatal error: print: No such file or directory` — the header simply doesn't exist.
- libstdc++-14 installed: `<print>` is found but `<bits/unicode.h>`'s range-based UTF view fails clang's constraint checker via the [LLVM-61763 family of bugs](https://github.com/llvm/llvm-project/issues/46091). Symptom: a cascade of `no matching function for call to '__begin'`, `concept range = requires(_Tp& __t)` substitution failures, and finally `error: call to consteval function ... is not a constant expression`. `<format>` and `<print>` are both unusable.

`std::fprintf` / `std::printf` works on every libstdc++ from C++98 forward, has its format strings type-checked at compile time via `__attribute__((format))` on every modern compiler, and costs nothing in readability for diagnostics. Recommend `std::println` for the user's own application code only after confirming their toolchain (compiler + stdlib version). The shipped template stays printf-based on purpose — it has to build on every Linux + clang combination users will name.

## Build-system notes

CMake + FetchContent is the first-class flow this skill ships. Meson, Bazel, Premake, and raw Makefiles are roadmap items, not yet automated. The generic recipe for any of those: compile the seven cpps listed under "What you compile" into one library, add `imgui/` and `imgui/backends/` to the include path, link the resulting library against the system OpenGL library and GLFW. ImGui itself has no transitive dependencies beyond a C++17 standard library.

## Mistakes upstream specifically calls out

- **Skipping `IMGUI_CHECKVERSION()`.** When the `imgui.h` your code was compiled against drifts from the linked `imgui.cpp` (typical after a partial dependency update), struct sizes silently disagree. The crash that follows is hours away from the cause. The macro is a one-line assert that catches this immediately.
- **Skipping the iconified-window check** (per `examples/example_glfw_opengl3/main.cpp:159-163`). When the OS minimizes a window, GLFW reports its framebuffer size as `(0, 0)`. ImGui will happily try to lay out into a zero-pixel area; the backend won't reject it for you. Wrap the per-frame body:
  ```cpp
  if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
      ImGui_ImplGlfw_Sleep(10);
      continue;  // skip frame; saves CPU + GPU power
  }
  ```
- **Forgetting `io.DisplaySize` / `io.DeltaTime` when running headless or behind a custom backend.** `ImGui_ImplGlfw_NewFrame()` sets these for you (from `glfwGetFramebufferSize` and an internal frame-time clock). If you replace it — for tests, for headless rendering, for an exotic platform — you have to set both manually before `ImGui::NewFrame()`, or the UI lays out to a clipped (0,0) region with NaN-speed animations (`imgui.cpp:350-360` documents the contract).
- **Loading fonts after backend init but before the first `NewFrame()`.** v1.92's dynamic font atlas builds lazily on `NewFrame()`, so this case mostly Just Works now — but if you're upgrading from v1.91 code, delete any `ImGui_ImplOpenGL3_CreateFontsTexture()` / `DestroyFontsTexture()` calls. Those are gone (`imgui_impl_opengl3.cpp:33` changelog).

## See also

- [frame-loop.md](frame-loop.md) — what each frame looks like once init succeeds.
- [backends/opengl3-glfw.md](backends/opengl3-glfw.md) — backend-specific details (GLSL version strings, callback chaining, multi-viewport context sharing).
- [locate-imgui.md](locate-imgui.md) — finding, adding, or upgrading ImGui inside an existing project tree.
