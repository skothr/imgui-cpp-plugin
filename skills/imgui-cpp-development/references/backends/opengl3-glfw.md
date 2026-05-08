# Backend: OpenGL 3 + GLFW

> **Load this file when:** the project's renderer is OpenGL 3 (or OpenGL ES 2/3) and the platform layer is GLFW. For other backends, the corresponding doc isn't in v1; fall back to upstream's `examples/example_<name>/main.cpp` and `imgui_impl_<name>.{h,cpp}`.

<!-- QUICK_NAV_BEGIN -->
> **Quick navigation** (jump to a section instead of loading the whole file - `Read offset=N limit=M`):
>
> - L  22-25   What this backend is
> - L  26-54   Public API surface
> - L  55-89   Init and shutdown
> - L  90-102  The `glsl_version` argument
> - L 103-123  Per-frame ordering
> - L 124-131  GL loader
> - L 132-150  `ImTextureID` and `ImTextureRef`
> - L 151-171  Multi-viewport on GLFW
> - L 172-199  Common pitfalls
> - L 200-217  Bundled main.cpp template
> - L 218-222  See also
<!-- QUICK_NAV_END -->



## What this backend is

The "GLFW + OpenGL 3" pairing is two independent ImGui backend libraries cooperating around one window. **GLFW** owns OS-level concerns: window creation, the GL context, keyboard/mouse/gamepad/clipboard plumbing, monitor enumeration, DPI. **`imgui_impl_glfw`** is the *platform* backend — it installs GLFW callbacks (or asks you to forward to per-event functions) and translates them into `ImGuiIO` events (`AddKeyEvent`, `AddMousePosEvent`, etc.), and each frame it pushes display size and frame timing into ImGui. **`imgui_impl_opengl3`** is the *renderer* backend — it owns the shader, VAO/VBO, and font-atlas texture, and at end-of-frame it walks ImGui's `ImDrawData` and emits indexed triangles through the OpenGL 3.x pipeline (also works for ES 2/3 with the right `#define`). Neither library knows about the other; they're glued together by your `main()`. That separation is why you can swap GLFW out for SDL3, Win32, or anything else without touching renderer code, and swap OpenGL out for Vulkan/DX/Metal/WebGPU without touching platform code.

## Public API surface

### Platform: `imgui_impl_glfw.h`

| Function | Purpose |
|---|---|
| `ImGui_ImplGlfw_InitForOpenGL(window, install_callbacks)` | Initialise platform backend; pair with `ImGui_ImplOpenGL3_*`. (`imgui_impl_glfw.h:35`) |
| `ImGui_ImplGlfw_InitForVulkan(window, install_callbacks)` | Same, paired with the Vulkan renderer backend. (`imgui_impl_glfw.h:36`) |
| `ImGui_ImplGlfw_InitForOther(window, install_callbacks)` | Same, for renderers we don't have a dedicated init for (DX, Metal, WebGPU, custom). (`imgui_impl_glfw.h:37`) |
| `ImGui_ImplGlfw_Shutdown()` | Tear down; restores prior GLFW callbacks if it installed any. (`imgui_impl_glfw.h:38`) |
| `ImGui_ImplGlfw_NewFrame()` | Per-frame: write `io.DeltaTime`, `io.DisplaySize`, mouse/key state. (`imgui_impl_glfw.h:39`) |
| `ImGui_ImplGlfw_InstallCallbacks(window)` | Manually attach callbacks (when `install_callbacks=false` was passed). (`imgui_impl_glfw.h:50`) |
| `ImGui_ImplGlfw_RestoreCallbacks(window)` | Detach them. (`imgui_impl_glfw.h:51`) |
| `ImGui_ImplGlfw_SetCallbacksChainForAllWindows(bool)` | Chain callbacks through every GLFW window, not just the main. Needed for some multi-viewport setups. (`imgui_impl_glfw.h:55`) |
| `ImGui_ImplGlfw_WindowFocusCallback`, `…CursorEnterCallback`, `…CursorPosCallback`, `…MouseButtonCallback`, `…ScrollCallback`, `…KeyCallback`, `…CharCallback`, `…MonitorCallback` | The individual event hooks. Forward GLFW events to these from your own callbacks when you opted out of `InstallCallbacks`. (`imgui_impl_glfw.h:58–65`) |
| `ImGui_ImplGlfw_Sleep(ms)` | Polite sleep for iconified-window idle. (`imgui_impl_glfw.h:68`) |
| `ImGui_ImplGlfw_GetContentScaleForWindow(w)` / `…ForMonitor(m)` | DPI scale factor (GLFW 3.3+); use to seed `style.ScaleAllSizes` and `style.FontScaleDpi`. (`imgui_impl_glfw.h:69–70`) |

### Renderer: `imgui_impl_opengl3.h`

| Function | Purpose |
|---|---|
| `ImGui_ImplOpenGL3_Init(glsl_version = nullptr)` | Compile shader, create VAO/VBO, set up the font atlas. `nullptr` auto-picks a GLSL version that matches your live GL context. (`imgui_impl_opengl3.h:35`) |
| `ImGui_ImplOpenGL3_Shutdown()` | Destroy GL resources owned by the backend. (`imgui_impl_opengl3.h:36`) |
| `ImGui_ImplOpenGL3_NewFrame()` | Lazy-rebuild any GPU objects that were destroyed (font atlas, shader). Cheap when nothing changed. (`imgui_impl_opengl3.h:37`) |
| `ImGui_ImplOpenGL3_RenderDrawData(draw_data)` | Walk the draw lists in `draw_data` and submit them to the current GL context. (`imgui_impl_opengl3.h:38`) |
| `ImGui_ImplOpenGL3_CreateDeviceObjects()` / `DestroyDeviceObjects()` | Manual control over GL resources; rarely needed — Init/NewFrame/Shutdown call these for you. (`imgui_impl_opengl3.h:41–42`) |
| `ImGui_ImplOpenGL3_UpdateTexture(tex)` | Push a single `ImTextureData` to the GPU. Called automatically during `RenderDrawData` unless you set `ImDrawData::Textures = nullptr` to drive the dynamic-atlas updates yourself. (`imgui_impl_opengl3.h:45`) |

## Init and shutdown

The order is fixed by data dependencies: GLFW must be running before you can ask for a GL context; the GL context must be current before either backend can issue GL calls or wire callbacks; ImGui must have a context before backends can register themselves on it. Reverse the order on shutdown.

```cpp
glfwSetErrorCallback(glfw_error_callback);
if (!glfwInit()) return 1;

apply_window_hints();              // GL version, profile, forward-compat — must precede glfwCreateWindow
GLFWwindow* window = glfwCreateWindow(1280, 800, "App", nullptr, nullptr);
glfwMakeContextCurrent(window);    // GL context must be current before either Init below
glfwSwapInterval(1);

IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable
               |  ImGuiConfigFlags_ViewportsEnable;   // set BEFORE backend init

ImGui_ImplGlfw_InitForOpenGL(window, /*install_callbacks=*/true);
ImGui_ImplOpenGL3_Init(/*glsl_version=*/nullptr);     // nullptr → auto-detect

// ... main loop ...

ImGui_ImplOpenGL3_Shutdown();
ImGui_ImplGlfw_Shutdown();
ImGui::DestroyContext();
glfwDestroyWindow(window);
glfwTerminate();
```

The bool to `ImGui_ImplGlfw_InitForOpenGL(window, install_callbacks)` controls who owns GLFW's input callbacks. Per the canonical comment at `imgui_impl_glfw.h:47–49`: with `true`, the backend installs GLFW callbacks and **chain-calls** any callbacks you had registered before, so you keep your handlers and ImGui still gets every event. With `false`, the backend installs nothing — you keep full ownership of `glfwSetKeyCallback` etc., but you must forward each GLFW event to the matching `ImGui_ImplGlfw_*Callback` (otherwise ImGui sees no input). What you "lose" by passing `false` is convenience: you trade one bool for the responsibility of routing eight callbacks correctly. Pass `true` unless you have a concrete reason to interpose before ImGui's event handling.

The `Push`/`Pop` and `Begin`/`End` discipline inside the frame is where `imscoped.hpp` earns its keep, but the init/shutdown calls themselves are unpaired — write them as plain function calls.

## The `glsl_version` argument

`ImGui_ImplOpenGL3_Init` takes an optional `"#version XXX"` string. The header's own comment (`imgui_impl_opengl3.h:25–28`) recommends `nullptr` and only overriding when your GL context can't handle the auto-picked version. The platform-specific defaults you'd choose if you do override:

| Target | GL context | GLSL string |
|---|---|---|
| macOS (3.2+ core profile) | GL 3.2 | `"#version 150"` |
| Linux/Windows desktop | GL 3.0+ | `"#version 130"` |
| Web / Emscripten (WebGL 1) | GL ES 2.0 | `"#version 100"` |
| Web / Emscripten (WebGL 2), iOS, Android | GL ES 3.0 | `"#version 300 es"` |

The full table is at `imgui_impl_opengl3.cpp:105–121`. The constraint is simply that the GLSL version you ask for must be a version your live GL context actually supports — set the GLFW window hints first (major/minor version, core profile on macOS, forward-compat on macOS), then either pass the matching string or pass `nullptr` and let the backend introspect `glGetString(GL_VERSION)`.

## Per-frame ordering

Every frame runs three `NewFrame` calls in this exact order:

```cpp
ImGui_ImplOpenGL3_NewFrame();   // 1. renderer: lazy-rebuild GPU resources if needed
ImGui_ImplGlfw_NewFrame();      // 2. platform: write io.DeltaTime, io.DisplaySize, input
ImGui::NewFrame();              // 3. core: clears per-frame state, opens UI submission
```

Renderer first because, since v1.92, dynamic font atlas updates can require GPU work to be staged before ImGui starts mutating the atlas during widget submission. Platform second because ImGui needs `io.DisplaySize` and `io.DeltaTime` populated *before* `ImGui::NewFrame()` runs layout. ImGui core last because it consumes whatever the backends just wrote. Calling them out of order is a common source of "first-frame is wrong, then it stabilises" bugs.

After UI submission, the render side is symmetric and tighter:

```cpp
ImGui::Render();                                            // finalize draw data
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());     // submit to GL
```

`GetDrawData()` is only valid between `Render()` and the next `NewFrame()`. Between those two calls, you set up your own GL state (viewport, clear color) and the backend submits its triangles on top of whatever you've drawn. The backend saves and restores the GL state it touches, but it does not clear the framebuffer or set `glViewport` — those are your responsibility.

## GL loader

You don't need GLEW, glad, or glbinding. The backend embeds a stripped GL3W-derived loader, `imgui_impl_opengl3_loader.h`, and pulls it in at `imgui_impl_opengl3.cpp:182–184`. The note at `imgui_impl_opengl3.cpp:174–181` describes when you'd regenerate it (only relevant if you're extending the backend itself).

To opt out — for instance, if your engine already links GLEW and you want to share that loader — define one of the `IMGUI_IMPL_OPENGL_LOADER_*` macros (`IMGUI_IMPL_OPENGL_LOADER_GLEW`, `…_GLAD`, `…_GLAD2`, `…_GLBINDING2`, `…_GLBINDING3`, or `…_CUSTOM`) globally or in `imconfig.h`. With `LOADER_CUSTOM`, you provide all the GL function pointers yourself before calling any ImGui GL function. Leave this alone unless you have a real conflict; the embedded loader is the path of least resistance.

For ES, the path is different — define `IMGUI_IMPL_OPENGL_ES2` or `IMGUI_IMPL_OPENGL_ES3` (auto-set on Emscripten/iOS/Android per `imgui_impl_opengl3.h:55–65`) and the backend skips the loader and pulls the platform's GLES headers directly.

## `ImTextureID` and `ImTextureRef`

For OpenGL, an ImGui texture identifier is just a `GLuint` widened to `ImU64` (the v1.92 default for `ImTextureID`). The header's first feature note is explicit (`imgui_impl_opengl3.h:7`): "User texture binding. Use 'GLuint' OpenGL texture as texture identifier."

```cpp
GLuint my_gl_tex = /* create + glTexImage2D somewhere */;

if (auto w = ImScoped::Window("Preview")) {
    // Old API, still supported: pass an ImTextureID directly.
    ImGui::Image(static_cast<ImTextureID>(my_gl_tex), ImVec2(256, 256));

    // v1.92 API: pass an ImTextureRef. Implicit construction from ImTextureID
    // works, so this is rarely more verbose.
    ImGui::Image(ImTextureRef(static_cast<ImTextureID>(my_gl_tex)), ImVec2(256, 256));
}
```

`ImTextureRef` (new in v1.92) wraps either a raw `ImTextureID` *or* an internal `ImTextureData*` pointer (used by the new dynamic font atlas — see the `ImGuiBackendFlags_RendererHasTextures` line at `imgui_impl_opengl3.h:9`). Most user-facing widgets that took `ImTextureID` now take `ImTextureRef`; your existing call sites keep compiling because `ImTextureRef` constructs implicitly from `ImTextureID`. The reason to care is that the renderer can defer the actual GPU upload — `ImTextureRef` resolves to the right `GLuint` only at draw time. For your own user textures (camera frame, image preview, render-to-texture), nothing changes: keep your `GLuint` alive across the frame, pass it through `ImTextureRef`, and the backend binds it via `glBindTexture(GL_TEXTURE_2D, …)` when it issues the draw call.

## Multi-viewport on GLFW

Multi-viewport lets ImGui windows escape the host window into independent OS windows. With OpenGL, that means each new GLFW window has its own GL context — and unless the contexts share resources, the textures, shaders, and VAOs you uploaded into the main context are invisible from secondary windows. GLFW exposes context sharing through the fourth argument to `glfwCreateWindow` (the `share` parameter); the backend uses this internally when it creates a viewport's window, so user code doesn't usually need to manage it. What user code *does* need to manage: setting your GLFW window hints (`GLFW_CONTEXT_VERSION_MAJOR/MINOR`, `GLFW_OPENGL_PROFILE`, `GLFW_OPENGL_FORWARD_COMPAT` on macOS) before *any* window is created, so secondary viewports inherit a compatible context.

After `ImGui_ImplOpenGL3_RenderDrawData`, drive the viewport pipeline:

```cpp
if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow* backup_current_context = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup_current_context);
}
```

The save/restore wrap is canonical — the comment at `examples/example_glfw_opengl3/main.cpp:216–218` says it directly:

> "Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere."

Without the restore, a later GL call in your own code lands on a secondary viewport's context and silently misbehaves. Two more setup details for stable multi-viewport rendering: set `ImGuiConfigFlags_ViewportsEnable` *before* the first `NewFrame()` (changing config flags mid-loop is unreliable), and zero out `style.WindowRounding` and force `Colors[ImGuiCol_WindowBg].w = 1.0f` so secondary OS windows look identical to the host (rounded corners with transparent backgrounds don't survive the round-trip through OS window manager).

## Common pitfalls

**Tiny squares instead of glyphs.** Symptom: text renders as a row of identical small rectangles. Root cause: the font atlas isn't where the shader expects it — either the atlas wasn't built, the atlas texture wasn't bound when the draw call ran, or your engine left a different shader/VAO bound after `RenderDrawData` and you're rendering through the wrong sampler. Fix: confirm `ImGui::GetIO().Fonts->IsBuilt()` is true; if you're loading custom fonts mid-session, call `io.Fonts->Build()` and let the next `ImGui_ImplOpenGL3_NewFrame()` re-upload. If the atlas is fine, audit your own GL state — `RenderDrawData` saves and restores its state, so the bug is usually that *your* code overwrites the binding before the next draw. The FAQ entry on this is the canonical reference (`docs/FAQ.md`, "I integrated Dear ImGui in my engine and little squares are showing instead of text").

**`ClipRect` interpreted as `(x, y, w, h)`.** Symptom: scissoring clips the wrong rectangle, widgets disappear, scroll regions render outside their parent. Root cause: `ImDrawCmd::ClipRect` is `(left, top, right, bottom)` packed into an `ImVec4`, not `(x, y, width, height)`. Fix: when implementing a custom GL state path, always derive width/height by subtraction:

```cpp
const float w = cmd->ClipRect.z - cmd->ClipRect.x;
const float h = cmd->ClipRect.w - cmd->ClipRect.y;
glScissor(int(cmd->ClipRect.x), int(fb_height - cmd->ClipRect.w), int(w), int(h));
```

The shipped backend gets this right; the trap is for code that walks `ImDrawData` itself.

**Iconified window pegs the CPU.** Symptom: minimising the window doesn't drop CPU usage; the loop keeps spinning at the swap interval. Root cause: `glfwPollEvents` returns immediately when nothing's happening and there's nothing to render to, so the loop spin-waits on a zero-area framebuffer. Fix: add the early-skip from upstream's example:

```cpp
glfwPollEvents();
if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
    ImGui_ImplGlfw_Sleep(10);
    continue;
}
```

This costs you one branch per frame and recovers full idle behaviour.

**Multi-viewport context lost after teardown.** Symptom: shutdown crashes inside `ImGui_ImplOpenGL3_Shutdown` (or first frame after a reload), GL functions return errors. Root cause: `RenderPlatformWindowsDefault` left a secondary viewport's context current, or the secondary GLFW window has already been destroyed but the backend still thinks its context is live. Fix: always run the `glfwMakeContextCurrent(backup_current_context)` line *every* frame the viewport pipeline ran (not conditionally), and shut down in the documented order — `ImGui_ImplOpenGL3_Shutdown` *before* `ImGui_ImplGlfw_Shutdown` *before* `ImGui::DestroyContext` *before* `glfwDestroyWindow`. The renderer backend touches GL during shutdown; the GL context must still be current.

## Bundled main.cpp template

Don't retype this from memory. The skill ships a working starter at `assets/main_glfw_opengl3.cpp.template` — it wires GLFW, ImGui, both backends, docking, multi-viewport, per-monitor DPI, and uses `ImScoped::*` guards in the frame loop. Copy it into a new project and adjust to taste. The frame-loop excerpt that demonstrates the imscoped pattern in this backend:

```cpp
ImGui_ImplOpenGL3_NewFrame();
ImGui_ImplGlfw_NewFrame();
ImGui::NewFrame();

if (auto win = ImScoped::Window("Hello, world!")) {
    ImGui::Text("Application: %.1f FPS", io.Framerate);
    ImGui::Checkbox("Show demo window", &show_demo_window);
    ImGui::ColorEdit3("Clear color", &clear_color.x);
}
```

`ImScoped::Window`'s destructor calls `ImGui::End()` on every path out of the block (including early return and exceptions). That's the entire reason the guard exists — there's no End() to forget regardless of whether `Begin()` returned true.

## See also

- [../bootstrap.md](../bootstrap.md) — overall project setup, file list, init order constraints (build-system-agnostic).
- [../frame-loop.md](../frame-loop.md) — the `NewFrame`/`Render` lifecycle in detail, including renderer-agnostic ordering.
- [../docking-and-viewports.md](../docking-and-viewports.md) — multi-viewport configuration and DockSpace mechanics.
