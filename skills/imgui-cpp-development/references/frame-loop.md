# Frame loop

> **Load this file when:** the user is writing or debugging the per-frame work between `Init()` and `Shutdown()` — the order of `NewFrame` / `Render` / `RenderDrawData` calls, multi-viewport additions, the Begin/End asymmetry, input dispatch via `WantCapture*`, or "my widgets don't appear / appear twice / corrupt the ID stack" symptoms that turn out to be lifecycle bugs.

This reference covers the canonical per-frame sequence, the one Begin/End asymmetry worth memorizing, how to dispatch input between ImGui and your app, the multi-viewport additions, and the small set of lifecycle pitfalls that cause silent rendering breakage. Build wiring and one-time init/shutdown live in [bootstrap.md](bootstrap.md).

## The required sequence

Per `imgui.cpp:296-310` (Programmer Guide skeleton) and `examples/example_glfw_opengl3/main.cpp:153-227`, every frame runs exactly:

```
poll OS events                      glfwPollEvents()
renderer NewFrame                   ImGui_ImplOpenGL3_NewFrame()
platform NewFrame                   ImGui_ImplGlfw_NewFrame()
ImGui::NewFrame()                   <-- after this, you may submit UI
... user UI submission ...
ImGui::Render()                     <-- finalizes draw lists
renderer RenderDrawData             ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData())
(if multi-viewport) UpdatePlatformWindows + RenderPlatformWindowsDefault
swap buffers                        glfwSwapBuffers(window)
```

Order matters. Backend `NewFrame` calls have to run before `ImGui::NewFrame` because they're what populate `io.DeltaTime`, `io.DisplaySize`, mouse position, and similar state that `NewFrame` then snapshots. Submitting widgets before `ImGui::NewFrame()` corrupts the per-frame state ImGui just reset.

`ImGui::Render()` finalizes draw data; `GetDrawData()` is valid only between `Render()` and the next `NewFrame()` (`imgui.h:411`). Calling `Render()` and then forgetting `RenderDrawData()` is a common silent failure: ImGui has finished its work but nothing reaches the GPU.

## The Begin/End asymmetry — the one rule worth memorizing

There is exactly one inconsistency in the Begin/End API surface, and the upstream comment at `imgui.h:436-440` (and again at `:458-462` for `BeginChild`) is blunt about it:

> Begin() return false to indicate the window is collapsed or fully clipped, so you may early out and omit submitting anything to the window. Always call a matching End() for each Begin() call, regardless of its return value!
>
> [Important: due to legacy reason, Begin/End and BeginChild/EndChild are inconsistent with all other functions such as BeginMenu/EndMenu, BeginPopup/EndPopup, etc. where the EndXXX call should only be called if the corresponding BeginXXX function returned true. Begin and BeginChild are the only odd ones out. Will be fixed in a future update.]

So:

- `Begin` / `End` and `BeginChild` / `EndChild` — `End*` runs **regardless** of the return value.
- Everything else (`BeginMenu`/`EndMenu`, `BeginPopup`/`EndPopup`, `BeginCombo`/`EndCombo`, `BeginListBox`/`EndListBox`, `BeginTable`/`EndTable`, `BeginTabBar`/`EndTabBar`, `BeginTabItem`/`EndTabItem`, `BeginTooltip`/`EndTooltip`, `BeginDragDropSource`/`EndDragDropSource`, `BeginDragDropTarget`/`EndDragDropTarget`) — `End*` runs **only when the matching Begin returned true**.

The `ImScoped::*` guards in `assets/imscoped.hpp` encode this difference at the type level. `ImScoped::Window` and `ImScoped::Child` always end on destruction; everything else conditionally ends. If you write the calls by hand, getting this wrong is a real `IM_ASSERT` (debug builds) or stack corruption (release).

## A worked frame using the scope guards

```cpp
ImGui_ImplOpenGL3_NewFrame();
ImGui_ImplGlfw_NewFrame();
ImGui::NewFrame();

// ImScoped::Window: End() runs in ~Window unconditionally,
// even if `tools.open` is false (collapsed) or we early-return below.
if (auto tools = ImScoped::Window("Tools")) {
    if (engine.is_busy()) {
        ImGui::TextUnformatted("Working...");
        return;  // safe — End() runs on stack unwind
    }
    if (ImGui::Button("Reload")) engine.reload();
    ImGui::SliderFloat("Speed", &engine.speed, 0.0f, 10.0f);
}

// ImScoped::Menu: EndMenu() runs only if BeginMenu returned true,
// because that matches ImGui's conditional-end contract for menus.
if (auto file = ImScoped::Menu("File")) {
    if (ImGui::MenuItem("Open"))  open_file();
    if (ImGui::MenuItem("Save"))  save_file();
}

ImGui::Render();
glfwGetFramebufferSize(window, &fb_w, &fb_h);
glViewport(0, 0, fb_w, fb_h);
glClear(GL_COLOR_BUFFER_BIT);
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
```

## Input dispatch: `io.WantCaptureMouse` / `io.WantCaptureKeyboard`

When ImGui owns the mouse (hovering an ImGui window, dragging a widget, an `InputText` is active), your application should not also handle that input. `io.WantCaptureMouse` and `io.WantCaptureKeyboard` (set inside `NewFrame`) tell you when to suppress dispatch (`imgui.h:2650-2651`):

```cpp
glfwPollEvents();
ImGui_ImplOpenGL3_NewFrame();
ImGui_ImplGlfw_NewFrame();
ImGui::NewFrame();

if (!io.WantCaptureMouse)    game.handle_mouse_input(...);
if (!io.WantCaptureKeyboard) game.handle_keyboard_input(...);
```

Always pass input *to* ImGui (the backend's installed callbacks do this for you). The `WantCapture*` flags only control whether you *also* dispatch it to your app. Don't use `IsWindowHovered` / `IsAnyWindowFocused` for this — `imgui.h:472` and `:1490` redirect to `WantCaptureMouse` explicitly.

## When you replace the GLFW backend (custom / headless)

`ImGui_ImplGlfw_NewFrame()` writes `io.DeltaTime`, `io.DisplaySize`, mouse and gamepad state into ImGui. If you skip that backend (custom platform, test harness, headless renderer), you set the contract manually before `ImGui::NewFrame()`, per the custom-backend skeleton in `imgui.cpp`:

```cpp
io.DeltaTime    = 1.0f / 60.0f;     // seconds since last frame
io.DisplaySize  = ImVec2(1920, 1080);
io.AddMousePosEvent(mx, my);
io.AddMouseButtonEvent(0, lmb_down);
```

Forgetting `DisplaySize` clips the entire UI to a (0,0) region. Forgetting `DeltaTime` produces zero-speed or NaN animations, depending on prior state.

## Multi-viewport: two extra calls and a context backup

When `io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable` is set, ImGui windows can detach into separate OS windows. After your main `RenderDrawData`, you give ImGui a turn to update and render its platform windows. From `examples/example_glfw_opengl3/main.cpp:219-225`:

```cpp
ImGui::Render();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow* backup = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();          // create/destroy/move OS windows
    ImGui::RenderPlatformWindowsDefault();   // render to each one
    glfwMakeContextCurrent(backup);          // restore: secondary windows changed it
}

glfwSwapBuffers(window);
```

The context backup/restore is required because `RenderPlatformWindowsDefault` calls `glfwMakeContextCurrent` for each secondary window and leaves the last one current. Without the restore, the next frame's GL state binds to whichever secondary window happened to render last.

## Pitfalls

- **`NewFrame` called twice without a `Render` between them.** ImGui asserts (`IM_ASSERT(g.WithinFrameScope == false)`) but only in debug builds. In release the second `NewFrame` corrupts internal state and you get garbage. Most often caused by a bug in a code path that early-returns past `Render()` on some condition — fix the early return, don't suppress the assert.
- **Submitting widgets before `NewFrame`.** Same root cause: a code path that runs UI code outside the `NewFrame` / `Render` window. `ImGui::Begin` will assert in debug and corrupt window state in release.
- **`Render()` without `RenderDrawData()`.** The frame submitted state but nothing reached the GPU; you'll see a frozen previous frame or a flat clear color. Easy to introduce when refactoring rendering into a separate function and forgetting to wire the backend call.
- **Using `IsWindowHovered` / `IsAnyItemActive` to gate app input.** Use `io.WantCaptureMouse` / `io.WantCaptureKeyboard`. The window-hover/focus helpers don't account for popups, modals, drag-and-drop, or active text input, all of which need ImGui to keep eating events.

## See also

- [bootstrap.md](bootstrap.md) — what `Init` / `Shutdown` look like around the loop.
- [docking-and-viewports.md](docking-and-viewports.md) — when (and when not) to enable `DockingEnable` / `ViewportsEnable`, and the constraints multi-viewport adds beyond the two-line render addition above.
- [backends/opengl3-glfw.md](backends/opengl3-glfw.md) — what each backend's `NewFrame` actually does, callback installation, GL state-restore details.
