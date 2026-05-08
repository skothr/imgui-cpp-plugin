> **Load this file when:** triaging a "why isn't ImGui doing what I expect" question and you don't yet know which subsystem is at fault. This is the cross-cutting index — each entry maps a symptom to the deep-dive doc that has the full explanation and fix.

Each row below is one well-known ImGui pitfall. Follow the doc link for code reproducer, root cause, and canonical fix. The pitfalls are grouped by area; within a group they're ordered by frequency of the question coming up.

## ID stack

| Symptom | Root cause | Deep dive |
|---|---|---|
| "All my buttons in a loop do the same thing" | Same label across loop iterations → identical IDs → ImGui sees one button. Wrap each iteration in `ImScoped::ID{&item}` or `ImScoped::ID{i}`. | [id-stack.md](id-stack.md) |
| "Two windows with the same widget label share state" | The widget is identified by `(window-id × label)`; if two `Begin("X")` calls exist (typo, two callers), they hash to the same window ID and child IDs collide. Find and rename the duplicate `Begin`. | [id-stack.md](id-stack.md) |
| "I changed a label and the widget lost its state" | Label change ⇒ new hash ⇒ new ID ⇒ ImGui treats it as a fresh widget. Use `"Visible label###stable-id"` to keep the ID stable across label changes. | [id-stack.md](id-stack.md) |
| ".ini settings stopped applying after upgrade to v1.92.6+" | The ID hash function changed in v1.92.6; saved IDs no longer match. Re-save the .ini, or live with the one-time reset. | [changelog-1.92.x.md](changelog-1.92.x.md) |
| `IM_ASSERT(g.IDStack.Size > 1)` fires on shutdown | Unbalanced PushID/PopID. Use `ImScoped::ID` to make the pairing automatic. | [raii-scope-guards.md](raii-scope-guards.md) |

## Begin/End pairing

| Symptom | Root cause | Deep dive |
|---|---|---|
| `IM_ASSERT(window->WriteAccessed)` or "current window stack" assert at frame end | Missing `End()` after a `Begin()`. Begin/End and BeginChild/EndChild must always be paired regardless of return value. Use `ImScoped::Window`/`Child`. | [frame-loop.md](frame-loop.md), [raii-scope-guards.md](raii-scope-guards.md) |
| Invisible / corrupted widgets after a popup closes | Calling `EndPopup()` when `BeginPopup()` returned false. For all Begin* APIs *except* Begin/BeginChild, you only call End* when the corresponding Begin* returned true. Use `ImScoped::Popup`. | [modals-and-popups.md](modals-and-popups.md) |
| "My table doesn't render" | `EndTable()` skipped because `BeginTable()` returned false (e.g. ScrollY without a sized outer area). Use `ImScoped::Table` and read the table's outer-size requirements. | [tables.md](tables.md) |

## Sizing and layout

| Symptom | Root cause | Deep dive |
|---|---|---|
| "My child window keeps growing one pixel each frame" | `ImGuiChildFlags_AutoResizeY` plus content whose size depends on the child's measured size = positive feedback loop. Pin a fixed height, use `SetNextWindowSizeConstraints`, or make the child user-resizable. | [layout-and-sizing.md](layout-and-sizing.md) |
| "My `SetNextWindowSize` is ignored" | Three usual causes: (1) called *after* `Begin`, (2) `.ini` state has it pinned (use `ImGuiCond_Always`), (3) `ImGuiWindowFlags_AlwaysAutoResize` is set on the window. | [layout-and-sizing.md](layout-and-sizing.md) |
| "Scrollbar disappears or won't appear" | Scrollbar requires content overflow on a non-auto-resize axis. `ImGuiWindowFlags_HorizontalScrollbar` must be passed via `window_flags`. Don't combine `AutoResize*` with scrolling on the same axis. | [layout-and-sizing.md](layout-and-sizing.md) |
| "Padding looks doubled / spacing wrong" | `style.ScaleAllSizes(factor)` was called twice (it bakes scale destructively). Don't call it more than once per context. | [styling-fonts-dpi.md](styling-fonts-dpi.md) |
| "GetContentRegionAvail returns garbage" | Called from outside an active window scope, or before the window has been laid out for the first time. Call it inside `Begin`/`End`, after at least one item. | [layout-and-sizing.md](layout-and-sizing.md) |

## Frame loop / event handling

| Symptom | Root cause | Deep dive |
|---|---|---|
| "ImGui doesn't get my mouse clicks" / "my game gets clicks ImGui should have eaten" | You're not checking `io.WantCaptureMouse` / `io.WantCaptureKeyboard` before dispatching to the application. ImGui only sees inputs you forward to it; the app should suppress its own handling when ImGui wants the input. | [frame-loop.md](frame-loop.md) |
| Crash inside `NewFrame` after window minimization | Iconified windows get DisplaySize 0 or negative; some code paths assume positive. Skip the frame entirely while iconified. | [bootstrap.md](bootstrap.md), [backends/opengl3-glfw.md](backends/opengl3-glfw.md) |
| `IM_ASSERT(g.WithinFrameScope)` fires | Calling ImGui submission functions outside `NewFrame`/`Render`. Or you called `NewFrame` twice without `Render` between them. | [frame-loop.md](frame-loop.md) |
| Input events lost after dragging window between monitors | Multi-viewport context loss; the save/restore wrap around `UpdatePlatformWindows` / `RenderPlatformWindowsDefault` is missing. | [docking-and-viewports.md](docking-and-viewports.md), [backends/opengl3-glfw.md](backends/opengl3-glfw.md) |

## Fonts & DPI

| Symptom | Root cause | Deep dive |
|---|---|---|
| Tiny squares instead of glyphs (the missing-glyph tofu boxes) | Font atlas not built or texture not bound. In v1.92 the atlas builds automatically on `NewFrame`; if you've called the legacy explicit `CreateFontsTexture()` with a v1.92-aware backend, you may double-build. | [styling-fonts-dpi.md](styling-fonts-dpi.md), [backends/opengl3-glfw.md](backends/opengl3-glfw.md) |
| Blurry / fuzzy text on Retina or 4K | DPI scaling not applied before backend init. Set `style.FontScaleDpi` and call `style.ScaleAllSizes` BEFORE `ImGui_Impl*_Init`. | [styling-fonts-dpi.md](styling-fonts-dpi.md) |
| Fonts vanish after switching monitors | Atlas not rebuilt for the new DPI. Set `io.ConfigDpiScaleFonts = true` (v1.92, dynamic atlas). | [styling-fonts-dpi.md](styling-fonts-dpi.md) |
| Icon font glyphs show as `?` | `MergeMode` not set on the icon `ImFontConfig`, or merged glyph ranges don't cover the icons' codepoints. | [styling-fonts-dpi.md](styling-fonts-dpi.md) |
| `AddFontFromMemoryTTF` font dies after window resize | `FontDataOwnedByAtlas = false` requires the data to outlive the atlas; if you free it, the atlas's pointer dangles. Either keep the data alive or switch to `true` (atlas owns + frees). | [styling-fonts-dpi.md](styling-fonts-dpi.md) |

## Style stack

| Symptom | Root cause | Deep dive |
|---|---|---|
| `IM_ASSERT(g.ColorStack.Size == ...)` at frame end | Unbalanced PushStyleColor / PopStyleColor. Use `ImScoped::StyleColor` to make pairing automatic. | [raii-scope-guards.md](raii-scope-guards.md), [styling-fonts-dpi.md](styling-fonts-dpi.md) |
| Style change "leaks" into other widgets | Forgot to PopStyleVar/Color (or popped the wrong count). The same RAII guard fixes this. | [raii-scope-guards.md](raii-scope-guards.md) |

## Modals & popups

| Symptom | Root cause | Deep dive |
|---|---|---|
| "My popup never closes" | `OpenPopup("X")` is called every frame, re-arming the popup each time. Call `OpenPopup` once on a triggering event (button click, etc.). | [modals-and-popups.md](modals-and-popups.md) |
| Modal "X" close button does nothing | Didn't pass a `bool* p_open` to `BeginPopupModal`; with no pointer to toggle, ImGui has no way to signal "close". | [modals-and-popups.md](modals-and-popups.md) |
| Right-click context menu fires twice / collides | Two `BeginPopupContextItem` / `BeginPopupContextWindow` calls overlap. Use distinct `str_id` values, or push an ID before the conflicting region. | [modals-and-popups.md](modals-and-popups.md) |

## Tables

| Symptom | Root cause | Deep dive |
|---|---|---|
| "My selectable doesn't span the row" | Missing `ImGuiSelectableFlags_SpanAllColumns`. | [tables.md](tables.md) |
| "Frozen rows aren't sticky" | `TableSetupScrollFreeze` requires `ImGuiTableFlags_ScrollY` (otherwise nothing scrolls, so nothing freezes). | [tables.md](tables.md) |
| Sort spec data garbled | `TableGetSortSpecs` return is only valid during the current frame. Read, sort, and clear `SpecsDirty` within the same frame. | [tables.md](tables.md) |
| Table grows to fill way too much space | `outer_size = ImVec2(0, 0)` means content-fit; `(0, -FLT_MIN)` or `(0, height)` for explicit. | [tables.md](tables.md) |

## Custom widgets / drawlist

| Symptom | Root cause | Deep dive |
|---|---|---|
| "Next widget overlaps mine" | Custom widget didn't call `ItemSize` to reserve layout space. | [custom-widgets.md](custom-widgets.md) |
| Custom widget can't be hovered or focused | Skipped `ItemAdd`. The item protocol is `ItemSize` → `ItemAdd` → `ButtonBehavior` → draw. | [custom-widgets.md](custom-widgets.md) |
| Manual hit-testing misses popup occlusion | Used `IsMouseHoveringRect` instead of `IsItemHovered`. The framework function respects ImGui's overlay/popup occlusion; the manual rect test does not. | [custom-widgets.md](custom-widgets.md) |
| Drawing appears in the wrong window | Called `GetWindowDrawList()` after `End()`. The active window is gone; you got a stale list. | [custom-widgets.md](custom-widgets.md) |
| `ImTextureID` cast crashes the renderer | The concrete type varies per backend (OpenGL: `GLuint`; DX11: `ID3D11ShaderResourceView*`). Type errors are silent until a draw call. | [custom-widgets.md](custom-widgets.md) |

## Multi-viewport / docking

| Symptom | Root cause | Deep dive |
|---|---|---|
| Windows can't be dragged out of the host | `ImGuiConfigFlags_ViewportsEnable` isn't set, OR the backend doesn't advertise `BackendFlags_PlatformHasViewports` + `RendererHasViewports`. | [docking-and-viewports.md](docking-and-viewports.md) |
| Layout resets every launch | `.ini` save not happening (e.g., disabled `io.IniFilename`), or DockBuilder rebuilds layout every frame instead of just on first frame. | [docking-and-viewports.md](docking-and-viewports.md) |
| Black/transparent corners on popped-out windows | `style.WindowRounding` non-zero plus translucent `WindowBg` confuses the OS compositor. Set both to opaque when `ConfigFlags_ViewportsEnable` is on. | [docking-and-viewports.md](docking-and-viewports.md) |
| Mouse coords drift between monitors | Backend not handling `Platform_GetWindowDpiScale` correctly. GLFW backend handles this when `BackendFlags_PlatformHasViewports` is set. | [backends/opengl3-glfw.md](backends/opengl3-glfw.md) |

## Backend (OpenGL3 + GLFW)

| Symptom | Root cause | Deep dive |
|---|---|---|
| Black screen after window opens | Forgot to call `glClear` before `RenderDrawData`, OR called `RenderDrawData` before `ImGui::Render`. | [backends/opengl3-glfw.md](backends/opengl3-glfw.md) |
| Application 100% CPU when minimized | Iconified-window check missing in the main loop. Skip `ImGui_Impl*_NewFrame` while `glfwGetWindowAttrib(window, GLFW_ICONIFIED)` is true. | [backends/opengl3-glfw.md](backends/opengl3-glfw.md) |
| GLSL compile errors at startup | Wrong `glsl_version` string passed to `ImGui_ImplOpenGL3_Init` for the GL context version. macOS = `"#version 150"`, desktop ≥3.0 = `"#version 130"`, GL ES 3 = `"#version 300 es"`. | [backends/opengl3-glfw.md](backends/opengl3-glfw.md) |
| Input callbacks fight ImGui's | `install_callbacks=true` but you also installed your own GLFW callbacks. Either chain them via the per-callback ImGui forwarders, or pass `install_callbacks=false` and forward events manually. | [backends/opengl3-glfw.md](backends/opengl3-glfw.md) |

## How to use this index

1. Match the symptom in the user's description to the closest row.
2. Load the linked deep-dive doc; don't try to fix from this catalog alone (it's an index, not a how-to).
3. If no row matches, check the FAQ in `vendor/imgui/docs/FAQ.md` directly — it covers a much wider surface and is updated upstream.

## See also

- [changelog-1.92.x.md](changelog-1.92.x.md) — pitfalls that came (or went) with specific v1.92.x releases.
- [lsp-navigation.md](lsp-navigation.md) — when in doubt, navigate to the upstream source. ImGui's monofiles are dense but well-commented.
