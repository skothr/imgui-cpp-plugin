# Docking and viewports

> **Load this file when:** building a multi-pane editor / IDE-style layout (DockSpace, dockable child windows, programmatic layout setup) or enabling multi-viewport support so ImGui windows can drag outside the host window into native OS windows. The docking branch is required; v1.92.x includes both features.
>
> **Tier guidance:** Tier 1 covers the 80% case — ConfigFlags, the two canonical DockSpace patterns, multi-viewport setup, and the common pitfalls. Tier 2 is programmatic layout via `DockBuilder` and the `SetNextWindowDockID` / `GetWindowDockID` / `IsWindowDocked` queries. Tier 3 is `ImGuiWindowClass`, .ini hooks, and per-viewport state. Default load: Tier 1 only via the Quick navigation below.

<!-- QUICK_NAV_BEGIN -->
> **Quick navigation** (jump to a section instead of loading the whole file - `Read offset=N limit=M`):
>
> - L  16-32   1. The two ConfigFlags — independent, not paired
> - L  33-84   2. DockSpace canonical patterns
> - L  85-112  3. Multi-viewport setup
> - L 113-134  4. Common pitfalls
> - L 135-165  5. DockBuilder — programmatic layout, with a stability caveat
> - L 166-179  6. Programmatic queries: SetNextWindowDockID, GetWindowDockID, IsWindowDocked
> - L 180-208  7. ImGuiWindowClass — class-scoped docking
> - L 209-224  8. .ini persistence — automatic, with manual hooks
> - L 225-238  9. Per-viewport state
> - L 239-243  See also
<!-- QUICK_NAV_END -->


The two features ship together but solve different problems. Docking lets ImGui windows merge into a tabbed/split layout inside a single host window. Multi-viewport lets ImGui detach a window into its own OS-level window so the user can drag it onto a second monitor. Real apps usually want both, but you can enable them independently — and most "nothing works" reports turn out to be a missing config flag or a missing backend flag, not a real bug.

---

# Tier 1 — Quick answers

## 1. The two ConfigFlags — independent, not paired

Per `imgui.h:1785` and `imgui.h:1789`, the relevant bits in `io.ConfigFlags` are:

- `ImGuiConfigFlags_DockingEnable` — turns on docking inside the main viewport. Drag a window's title bar onto another window or onto a `DockSpace()` to dock it. No backend support required.
- `ImGuiConfigFlags_ViewportsEnable` — turns on multi-viewport. Floating windows can leave the main viewport and become OS-level windows. Requires *both* `io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports` *and* `io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports` set by the platform/renderer backend (`imgui.h:1813-1814`). If either is missing, the flag is silently ignored — no error, no log, just no detached windows.

```cpp
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // tabbed docks inside main window
io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // drag windows onto other monitors
```

Pick docking-only for a single-window IDE layout. Pick docking + viewports for a "tear-out a panel onto monitor 2" workflow. Pick viewports-only (rare) when you want pop-out windows but no tabbed docks.

---

## 2. DockSpace canonical patterns

A dockspace is a region inside a window that can host other windows. Two patterns from the upstream comment block at `imgui.h:975-988`:

**Pattern A — full-window dockspace, one line.** `DockSpaceOverViewport` creates an invisible host window covering a viewport and submits a dockspace into it.

```cpp
ImGui::NewFrame();
ImGui::DockSpaceOverViewport();   // entire main viewport becomes a dockspace
// ... your windows submit normally; users drag them into the dockspace
```

For a transparent central area (so background art shows through), pass `ImGuiDockNodeFlags_PassthruCentralNode`:

```cpp
ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
```

**Pattern B — custom layout with menubar.** When you want a menubar, status bar, or your own window decorations around the dockspace, host it inside an explicit fullscreen window:

```cpp
const ImGuiViewport* vp = ImGui::GetMainViewport();
ImGui::SetNextWindowPos(vp->WorkPos);
ImGui::SetNextWindowSize(vp->WorkSize);
ImGui::SetNextWindowViewport(vp->ID);
ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

if (auto host = ImScoped::Window("##DockHost", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar)) {
    if (auto mb = ImScoped::MenuBar()) {
        if (auto m = ImScoped::Menu("File")) { ImGui::MenuItem("Quit"); }
    }
    ImGui::DockSpace(ImGui::GetID("MyDockspace"));
}
ImGui::PopStyleVar(3);
```

Two upstream invariants from `imgui.h:982-984`:

> Dockspaces need to be submitted *before* any window they can host. Submit them early in your frame.
>
> Dockspaces need to be kept alive if hidden, otherwise windows docked into it will be undocked. ... submit the non-visible dockspaces with `ImGuiDockNodeFlags_KeepAliveOnly`.

If you stop submitting a dockspace one frame, every window docked into it pops out. The fix for tabbed-page layouts is to keep submitting it with `KeepAliveOnly` while the tab is hidden.

---

## 3. Multi-viewport setup

A working multi-viewport setup needs four things, in order:

1. **App enables the config flag**: `io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;`
2. **Backends advertise support**: the platform backend sets `ImGuiBackendFlags_PlatformHasViewports`; the renderer backend sets `ImGuiBackendFlags_RendererHasViewports`. The shipped GLFW/OpenGL3 backends do this in their `Init()` paths automatically.
3. **Frame lifecycle calls** after `ImGui::Render()` (`imgui.h:1197-1198`):

   ```cpp
   ImGui::Render();
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

   if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
       GLFWwindow* prev_ctx = glfwGetCurrentContext();
       ImGui::UpdatePlatformWindows();          // create/resize/destroy OS windows
       ImGui::RenderPlatformWindowsDefault();   // render + swap each secondary viewport
       glfwMakeContextCurrent(prev_ctx);        // restore main GL context
   }

   glfwSwapBuffers(window);
   ```

4. **Style adjustment** — `style.WindowRounding = 0.0f;` and `style.Colors[ImGuiCol_WindowBg].w = 1.0f;` so popped-out OS windows don't draw transparent rounded corners against whatever is behind them on the desktop. The upstream comment at `imgui.h:1788` says the same thing: "When using viewports it is recommended that your default value for `ImGuiCol_WindowBg` is opaque (Alpha=1.0) so transition to a viewport won't be noticeable."

The GLFW context save/restore around `UpdatePlatformWindows`/`RenderPlatformWindowsDefault` matters because each secondary viewport has its own GL context; rendering them makes that context current, and your main loop assumes the main context is current after the call returns. Skip the save/restore and the next `glfwSwapBuffers(window)` swaps the wrong buffer or asserts.

---

## 4. Common pitfalls

**1. Nested DockSpace inside another window's body.** A window can host one dockspace, but submitting `DockSpace` inside an already-dockable child window produces undefined layout. Use `DockSpaceOverViewport` for the top-level case, and one explicit `DockSpace` per host window otherwise. Don't nest.

**2. Layout reset on first run.** With no `imgui.ini` present, every window appears at default position and nothing is docked. Either ship a default `imgui.ini` alongside the binary, or run the DockBuilder "first-frame init" pattern (Tier 2 §5) so the first launch is already laid out.

**3. Dock IDs change between sessions.** `ImGui::GetID("MyDockspace")` hashes the string in the current ID stack — a stable input. But if you compute IDs from runtime state (loop indices, heap pointers), the hash changes between runs and the .ini lookup misses. Always seed DockBuilder with string IDs hashed via `GetID("...")`.

**4. Multi-viewport: surprise context loss after dragging out a window.** Symptom: the next `glfwSwapBuffers` shows a black frame or the GL state is stale. Cause: forgot to save/restore the GL context around `UpdatePlatformWindows`/`RenderPlatformWindowsDefault`. Fix shown above in §3.

**5. Mouse coords drift across multi-viewport.** Symptom: hover and clicks land in the wrong window after a window is dragged onto monitor 2. Cause: `ImGuiBackendFlags_PlatformHasViewports` not set, so ImGui assumes a single coordinate space. The shipped GLFW backend sets the flag in `Init()` only when viewports are enabled; double-check at runtime with `IM_ASSERT(io.BackendFlags & ImGuiBackendFlags_PlatformHasViewports);`.

**6. Hidden tab dockspaces undocking their windows.** If a dockspace lives inside a `BeginTabItem` and that tab isn't selected, the dockspace stops being submitted — and ImGui reacts by undocking everything. Submit it with `ImGuiDockNodeFlags_KeepAliveOnly` when the tab is hidden.

**7. DPI mismatch between viewports on a multi-monitor setup.** Symptom: text on the secondary viewport renders at the wrong size, or scaled UI elements are crisp on one monitor and blurry on the other. Cause: `DpiScale` differs per viewport, but fonts/styles aren't being rescaled. Set `io.ConfigDpiScaleFonts = true` and `io.ConfigDpiScaleViewports = true` to opt into automatic rescaling; both replaced the old `ImGuiConfigFlags_DpiEnableScale*` flags in v1.92 (`imgui.h:1797-1798`).

**8. `io.ConfigDockingWithShift` confuses users.** Setting this flag to `true` inverts docking input: now the user must hold Shift to dock a dragged window. Default is `false` (drag = dock, hold Shift to disable docking). Pick one and document it; flipping it after release retrains every existing user.

---

# Tier 2 — Programmatic layout

## 5. DockBuilder — programmatic layout, with a stability caveat

`DockBuilder*` lives in `imgui_internal.h:3743-3756`. The header is explicit about the contract:

> The DockBuilderXXX functions are designed to *eventually* become a public API, but it is too early to expose it and guarantee stability. ... If you intend to split the node immediately after creation using DockBuilderSplitNode(), make sure to call DockBuilderSetNodeSize() beforehand. ... Call DockBuilderFinish() after you are done.

So treat DockBuilder as a useful internal API that may change between minor versions. The "build once on first frame" recipe:

```cpp
static bool first_time = true;
const ImGuiID dock_id = ImGui::GetID("MyDockspace");

if (first_time) {
    first_time = false;
    ImGui::DockBuilderRemoveNode(dock_id);                              // wipe any prior layout
    ImGui::DockBuilderAddNode(dock_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock_id, ImGui::GetMainViewport()->Size);

    ImGuiID left, right;
    ImGui::DockBuilderSplitNode(dock_id, ImGuiDir_Left, 0.25f, &left, &right);
    ImGui::DockBuilderDockWindow("Inspector", left);
    ImGui::DockBuilderDockWindow("Viewport",  right);
    ImGui::DockBuilderFinish(dock_id);
}
ImGui::DockSpace(dock_id);
```

`DockBuilderSplitNode` returns the IDs of the two child nodes via out params; pass them to `DockBuilderDockWindow` by window name string. Call `DockBuilderFinish` last — without it, layout state is half-initialized and windows may dock to the wrong place.

---

## 6. Programmatic queries: SetNextWindowDockID, GetWindowDockID, IsWindowDocked

Per `imgui.h:991-994`:

- `SetNextWindowDockID(dock_id, cond)` — dock the next `Begin()` window into a node. `cond` (`ImGuiCond_FirstUseEver`, `_Once`, etc.) controls whether to override an existing dock assignment.
- `GetWindowDockID()` — the dock node ID of the current window, or 0 if floating.
- `IsWindowDocked()` — true when the current window is inside any dock node.

Useful when you want to disable a "pin to side" UI affordance while a window is already docked, or to record the layout for export.

---

# Tier 3 — Appendix

## 7. ImGuiWindowClass — class-scoped docking

The dock-class system (`imgui.h:2801-2814`) tags a window with a class so ImGui can refuse cross-class docks. Set `DockingAllowUnclassed = false` to make a class strict.

Use case: tools panels (Inspector, Outliner, Console) can dock to each other but should never dock onto the central 3D viewport.

```cpp
ImGuiWindowClass tools_class;
tools_class.ClassId = ImGui::GetID("ToolsClass");
tools_class.DockingAllowUnclassed = false;     // refuse to dock with the unclassed viewport

ImGui::SetNextWindowClass(&tools_class);
if (auto w = ImScoped::Window("Inspector")) { /* ... */ }

ImGui::SetNextWindowClass(&tools_class);
if (auto w = ImScoped::Window("Console"))   { /* ... */ }

// The 3D viewport is unclassed, so it can't accept these windows as dock targets.
if (auto w = ImScoped::Window("Viewport"))  { /* ... */ }
```

`SetNextWindowClass` must run *before* `Begin()` (or the `ImScoped::Window` constructor) for the class to take effect.

Beyond `ClassId` and `DockingAllowUnclassed`, the class struct (`imgui.h:2801-2814`) also exposes `DockingAlwaysTabBar` (force the window to always live inside a tab bar even when it's the only docked window), `ViewportFlagsOverrideSet`/`ViewportFlagsOverrideClear` (per-class control over OS decoration and task-bar icon when the window owns its own viewport), and `DockNodeFlagsOverrideSet` (experimental — override dock node flags for windows of this class). Treat the experimental fields as opt-in until you've verified the behavior on your target platforms.

The "auto-hide tab bar" pattern is unrelated to classes but lives near it conceptually: passing `ImGuiDockNodeFlags_AutoHideTabBar` (`imgui.h:1541`) when constructing a node makes the tab bar disappear when only one window is docked, and reappear once a second is added. Useful for "single-document with optional split" workflows.

---

## 8. .ini persistence — automatic, with manual hooks

When `io.IniFilename` is set (default `"imgui.ini"`, `imgui.h:2498`), ImGui auto-loads on first `NewFrame()` and auto-saves every `io.IniSavingRate` seconds. Docking layout, window position, size, and dock node tree all round-trip through the .ini file.

For explicit control (e.g. embedding layouts in your own project file format):

```cpp
io.IniFilename = nullptr;                                        // disable auto-save/load
ImGui::LoadIniSettingsFromMemory(buf.data(), buf.size());        // restore from string
const char* out = ImGui::SaveIniSettingsToMemory(nullptr);       // dump to string
```

Use `LoadIniSettingsFromMemory`/`SaveIniSettingsToMemory` when shipping a default layout with the binary, or to support per-user layouts stored in your DB. `LoadIniSettingsFromDisk`/`SaveIniSettingsToDisk` are the file-path equivalents.

---

## 9. Per-viewport state

`ImGuiViewport` (`imgui.h:4103-4129`) carries everything you need to position child windows correctly across monitors:

- `Pos`, `Size` — viewport position/size in OS desktop coordinates.
- `WorkPos`, `WorkSize` — the same minus OS task bars and menu bars; this is the rectangle your full-window dockspace should cover.
- `DpiScale` — `1.0f = 96 DPI`. Per-monitor; changes when the user drags between displays.
- `FramebufferScale` — Retina density (`(1,1)` on Windows, often `(2,2)` on macOS).
- `ParentViewportId` — backend hint for OS window hierarchy. Note: the GLFW backend doesn't honor this (`imgui_impl_glfw.cpp:17`).

`ImGui::GetMainViewport()` returns the primary viewport. Iterate every viewport via `ImGui::GetPlatformIO().Viewports`.

---

## See also

- [frame-loop.md](frame-loop.md) — where `UpdatePlatformWindows` / `RenderPlatformWindowsDefault` slot into the per-frame sequence.
- [backends/opengl3-glfw.md](backends/opengl3-glfw.md) — GLFW context handling and which backend flags get set when.
- [pitfalls-catalog.md](pitfalls-catalog.md) — cross-cutting index of "X breaks because Y" symptoms with deep-dive pointers.
