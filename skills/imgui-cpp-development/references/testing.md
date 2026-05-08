> **Load this file when:** writing automated tests against an ImGui-based application — interactive smoke tests during development or headless tests in CI. Covers imgui_test_engine integration: registration, the ItemClick / KeyDown / IM_CHECK API, GUI vs headless modes, and CMake wiring.

<!-- QUICK_NAV_BEGIN -->
> **Quick navigation** (jump to a section instead of loading the whole file - `Read offset=N limit=M`):
>
> - L  22-29   What this engine does (and what it doesn't)
> - L  30-50   Two run modes
> - L  51-68   Anatomy of a test
> - L  69-101  Registration
> - L 102-130  The test API surface
> - L 131-146  Named references — the path syntax
> - L 147-187  Initialization recipe
> - L 188-207  Headless run
> - L 208-235  Common patterns
> - L 236-244  Limitations and pitfalls
> - L 245-250  Recommended workflow
> - L 251-255  See also
<!-- QUICK_NAV_END -->


# Testing ImGui apps with imgui_test_engine

## What this engine does (and what it doesn't)

`imgui_test_engine` exercises the **interaction logic and UI state** of an ImGui application. It feeds simulated events into `ImGuiIO` (mouse position, button presses, key chords, character input) the same way the platform backend would, then runs your normal frame loop so the GuiFunc submits the UI under test. Test code drives those interactions through path-based references (`"Window/Button"`) and asserts via `IM_CHECK`.

It does **not** do pixel-level rendering verification. The engine knows nothing about whether your `ImDrawData` produced the right image — only whether the right items were submitted, hovered, clicked, and updated. For visual regression you need a separate pipeline (the engine ships an optional `ScreenCaptureFunc` you can wire to a backend's framebuffer readback if you want to graduate from logic to image diffing, but that's outside the engine's core concern).

The implication for test design: write assertions against your application's data model, not against rendered output. If clicking the OK button is supposed to set `state.confirmed = true`, that's the assertion. The engine's job is to make the click happen and pump frames; your job is to expose the state.

## Two run modes

The engine runs in one of two modes, chosen at startup via `ImGuiTestEngineIO::ConfigRunSpeed`:

- **GUI mode** — the engine appears as a window inside your live application (rendered by `ImGuiTestEngine_ShowTestEngineWindows`). You can pause, step, re-run, inspect logs interactively. This is the authoring mode: write a test, run it watch-speed, see where it goes wrong, fix it, re-run.
- **Headless mode** — no window. The engine pumps its own frames, runs the queued tests, exits with a return code based on the result summary. This is the CI mode.

The speed enum, from `imgui_te_engine.h:77-83`:

```cpp
enum ImGuiTestRunSpeed : int
{
    ImGuiTestRunSpeed_Fast          = 0,    // Run tests as fast as possible (teleport mouse, skip delays, etc.)
    ImGuiTestRunSpeed_Normal        = 1,    // Run tests at human watchable speed (for debugging)
    ImGuiTestRunSpeed_Cinematic     = 2,    // Run tests with pauses between actions (for e.g. tutorials)
    ImGuiTestRunSpeed_COUNT
};
```

`Fast` for headless, `Normal` for GUI authoring, `Cinematic` only when recording demo videos.

## Anatomy of a test

A test has up to three callbacks. From `imgui_te_engine.h:69-75`:

```cpp
enum ImGuiTestActiveFunc : int
{
    ImGuiTestActiveFunc_None,
    ImGuiTestActiveFunc_GuiFunc,            // == GuiFunc() handler
    ImGuiTestActiveFunc_TestFunc,           // == TestFunc() handler
    ImGuiTestActiveFunc_TeardownFunc,       // == TeardownFunc() handler
};
```

- **`GuiFunc`** submits the UI under test. It runs every frame the test is active (with two warmup frames before `TestFunc` starts, so the layout has settled). Treat it as the same UI submission code your real app would run — typically you share helpers between them. State that needs to flow between `GuiFunc` and `TestFunc` lives on `ctx->Test->UserData` (or via `test->SetVarsDataType<>()` + `ctx->GetVars<T>()` for typed access).
- **`TestFunc`** drives interactions and asserts. It runs as a coroutine: when it calls `ctx->Yield()` or any action that pumps frames (most of them), the engine steps to the next frame, runs `GuiFunc`, then resumes `TestFunc`.
- **`TeardownFunc`** runs once after the test, regardless of pass/fail. Use sparingly — most cleanup belongs in `UserData` constructors/destructors.

## Registration

The macro `IM_REGISTER_TEST(engine, "category", "test_name")` from `imgui_te_engine.h:207` returns an `ImGuiTest*` whose function pointers you populate. Typical pattern:

```cpp
struct CounterState { int counter = 0; };

void register_counter_test(ImGuiTestEngine* engine, CounterState* state) {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "demo", "counter_increments");
    t->UserData = state;

    t->GuiFunc = [](ImGuiTestContext* ctx) {
        auto* s = static_cast<CounterState*>(ctx->Test->UserData);
        if (auto w = ImScoped::Window("Hello, world!")) {
            if (ImGui::Button("Counter")) { s->counter++; }
            ImGui::Text("counter = %d", s->counter);
        }
    };

    t->TestFunc = [](ImGuiTestContext* ctx) {
        auto* s = static_cast<CounterState*>(ctx->Test->UserData);
        s->counter = 0;
        ctx->SetRef("Hello, world!");
        ctx->ItemClick("Counter");
        ctx->ItemClick("Counter");
        ctx->ItemClick("Counter");
        IM_CHECK_EQ(s->counter, 3);
    };
}
```

This is the same shape used by `assets/imgui_test_skeleton.cpp.template`, which is the file to start from rather than retyping by hand.

## The test API surface

Pulled from `imgui_te_context.h`. Names you'll reach for daily:

| Call | Purpose |
|---|---|
| `ctx->SetRef("Window/Path")` | Set the base path for subsequent ref lookups (`imgui_te_context.h:310`). |
| `ctx->ItemClick(ref)` | Move mouse to item, click LMB. Auto-opens parent menus/trees. |
| `ctx->ItemDoubleClick(ref)` | Double-click. |
| `ctx->ItemCheck(ref)` / `ItemUncheck(ref)` | Toggle a checkbox or any `Checkable` widget. |
| `ctx->ItemOpen(ref)` / `ItemClose(ref)` | Expand/collapse tree nodes, menus. |
| `ctx->ItemInput(ref)` | Activate a text input widget for typing. |
| `ctx->ItemAction(action, ref, flags)` | Generic dispatch — Hover, Click, DoubleClick, Check, Uncheck, Open, Close, Input, NavActivate. |
| `ctx->ItemInputValue(ref, v)` | Activate, type a value, commit (overloaded for `int`, `float`, `const char*`). |
| `ctx->ItemReadAsString(ref)` | Read the displayed value of a Slider/Drag/Input by selecting + clipboard round-trip. |
| `ctx->ItemExists(ref)` | Predicate; combined with `ImGuiTestOpFlags_NoError` for "is this widget present?" checks. |
| `ctx->MenuClick("File/Open")` | Drive menus (path is relative to current ref). |
| `ctx->KeyDown(chord)` / `KeyUp` / `KeyPress` | Keyboard input; `chord` accepts mod-or'd keys e.g. `ImGuiMod_Ctrl \| ImGuiKey_S`. |
| `ctx->KeyChars("text")` / `KeyCharsAppendEnter("text")` | Type text into focused input. |
| `ctx->MouseClick(button)` / `MouseMove(ref)` / `MouseDragWithDelta(delta)` | Raw mouse driving when `ItemClick` doesn't fit. |
| `ctx->Yield(count)` | Wait N frames; lets state settle, animations advance, popups open. |
| `ctx->Sleep(seconds)` / `SleepShort()` / `SleepStandard()` | Time-based wait; skipped in `Fast` mode. |
| `IM_CHECK(expr)` / `IM_CHECK_EQ(a, b)` / `IM_CHECK_NE(a, b)` / `IM_CHECK_LT(a, b)` | Assertions; on failure they log file:line and `return` from `TestFunc`. |
| `IM_CHECK_NO_RET(expr)` | Assert without returning — keeps the test going so you can collect more failures. |
| `IM_ERRORF(fmt, ...)` | Explicit error message from inside test logic. |
| `ctx->LogDebug(...)` / `LogInfo(...)` / `LogWarning(...)` / `LogError(...)` | Log routed through the engine's verbose system; visible in the GUI test runner and CI output. |

For the full surface, including docking helpers (`DockInto`, `UndockWindow`), tables (`TableSetColumnEnabled`, `TableClickHeader`), and viewports, read `imgui_te_context.h` directly — the file is well-commented and is the source of truth.

## Named references — the path syntax

Items are addressed by string paths, hashed under the hood. From `imgui_te_context.h:301-309`:

```
- ItemClick("Window/Button")                --> click "Window/Button"
- SetRef("Window"), ItemClick("Button")     --> click "Window/Button"
- SetRef("Window"), ItemClick("/Button")    --> click "Window/Button"
- SetRef("Window"), ItemClick("//Button")   --> click "/Button"
- SetRef("//$FOCUSED"), ItemClick("Button") --> click "Button" in focused window.
```

Plus wildcard support: `"**/Save"` searches any depth for an item named `"Save"`. Use `**` when a tree path is fragile or unknown; prefer absolute paths when stability matters.

A `SetRef` call may take multiple frames to resolve when given an item ID (it has to hover the window first). For window names that's instant.

## Initialization recipe

The bootstrap is the same in GUI and headless modes — only the frame pump differs. Walk through it once; from then on copy the skeleton.

1. Build ImGui with `IMGUI_ENABLE_TEST_ENGINE` defined. The bundled CMake template (`assets/CMakeLists-glfw-opengl3.txt.template`) has a commented FetchContent block; un-comment to enable:

```cmake
# FetchContent_Declare(imgui_test_engine
#     GIT_REPOSITORY https://github.com/ocornut/imgui_test_engine.git
#     GIT_TAG        v1.92.7
#     GIT_SHALLOW    TRUE)
# FetchContent_MakeAvailable(imgui_test_engine)
# add_library(imgui_te STATIC
#     ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_context.cpp
#     ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_coroutine.cpp
#     ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_engine.cpp
#     ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_exporters.cpp
#     ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_perftool.cpp
#     ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_ui.cpp
#     ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_utils.cpp)
# target_include_directories(imgui_te PUBLIC ${imgui_test_engine_SOURCE_DIR})
# target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_TEST_ENGINE)
# target_link_libraries(imgui_te PUBLIC imgui)
```

The `IMGUI_ENABLE_TEST_ENGINE` define has to be visible when `imgui.cpp` is compiled — that's why it lives on the `imgui` target as a `PUBLIC` definition. If you forget it, the engine builds, links, and silently does nothing.

2. Create the test engine context after `ImGui::CreateContext()` and start it, per `imgui_te_engine.h:199-203`:

```cpp
ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
ImGuiTestEngineIO& te_io = ImGuiTestEngine_GetIO(engine);
te_io.ConfigRunSpeed = headless ? ImGuiTestRunSpeed_Fast : ImGuiTestRunSpeed_Normal;
te_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
te_io.ConfigStopOnError  = false;
ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
ImGuiTestEngine_InstallDefaultCrashHandler();
```

3. Register tests, queue them, pump frames, get the summary, shut down. The bundled skeleton (`assets/imgui_test_skeleton.cpp.template`) has the full sequence wired up; load it rather than retyping.

## Headless run

The minimum frame pump for headless mode (no platform/renderer backend needed — the engine drives ImGui purely through the IO interface):

```cpp
ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, filter,
                           ImGuiTestRunFlags_RunFromCommandLine);
while (!ImGuiTestEngine_IsTestQueueEmpty(engine)) {
    ImGui::NewFrame();
    ImGuiTestEngine_ShowTestEngineWindows(engine, nullptr);
    ImGui::Render();
    ImGuiTestEngine_PostSwap(engine);
}
ImGuiTestEngineResultSummary summary;
ImGuiTestEngine_GetResultSummary(engine, &summary);
return summary.CountTested == summary.CountSuccess ? 0 : 1;
```

`PostSwap` is what advances the engine's coroutine — without it, `TestFunc` never resumes after a `Yield`. In a real GUI loop you call it after `SwapBuffers()` (or whatever your renderer's equivalent is).

## Common patterns

**Open a window, click a button, expect state change.** The basic shape — see the registration example above. The keys are: `SetRef` to anchor the path; `ItemClick` to drive the interaction; `IM_CHECK_EQ` against state owned by `UserData`.

**Type into an InputText, expect the bound variable to update.**

```cpp
t->TestFunc = [](ImGuiTestContext* ctx) {
    auto* s = static_cast<MyState*>(ctx->Test->UserData);
    ctx->SetRef("Settings");
    ctx->ItemInputValue("Name", "Ada Lovelace");
    IM_CHECK_EQ(strcmp(s->name, "Ada Lovelace"), 0);
};
```

`ItemInputValue` activates the field, replaces the contents, presses Enter. For more granular control use `ItemInput` + `KeyChars` / `KeyCharsAppend` / `KeyCharsReplaceEnter`.

**Drive a tree path.** Paths into nested widgets work straight through:

```cpp
ctx->ItemOpen("##list/Folder/Subfolder");
ctx->ItemClick("##list/Folder/Subfolder/Edit");
```

`ItemAction` auto-opens intermediate parents unless you pass `ImGuiTestOpFlags_NoAutoOpenFullPath`.

**Wait for animation or async settle.** `ctx->Yield()` advances one frame; `ctx->Yield(N)` advances N. For longer settling use `ctx->SleepStandard()` (skipped in `Fast` mode). `WaitForItemBlinkActive` exists for cases where the engine itself needs to wait for an item-highlight pulse to subside.

## Limitations and pitfalls

- **No pixel asserts.** The engine doesn't compare framebuffers. If "the button looks right" matters, that's a separate pipeline.
- **IDs change with layout, paths break.** Same widget label inside two different windows can collide; reordered docked windows produce different paths. When a test's path becomes brittle, give the widget a stable `"Label###unique-id"` override and target `"###unique-id"` directly. The `**` wildcard is a fallback when the ancestry is unstable.
- **Multi-viewport in headless mode is fiddly.** The mock viewport path doesn't fully exercise platform-window code. For headless CI runs, disable `ImGuiConfigFlags_ViewportsEnable` for the test build.
- **The engine maintains a coroutine.** If your `TestFunc` blocks on real I/O — disk, network, sleeping the thread — the engine's frame pump stalls. Anything that takes wall-clock time should be triggered by the GUI under test (so its progress is visible to `GuiFunc`) and waited on with `Yield` loops, not blocked on synchronously inside `TestFunc`.
- **`GuiFunc` runs every frame.** Side effects in it run repeatedly. Treat it as pure UI submission against `UserData`; do work elsewhere.
- **Conditional `End` discipline still applies.** The same Begin/End pairing rules that govern application code apply inside `GuiFunc`. Use `ImScoped::*` guards there too.

## Recommended workflow

Author tests interactively in GUI mode — start the app, find the test in the engine window, run it with `ImGuiTestRunSpeed_Normal`, watch where it diverges from your expectation, fix. When the test stabilizes, graduate it to the headless build by adding a registration call to your CI test main. CI runs `ImGuiTestRunSpeed_Fast`; expect a 10-100x speedup over watchable mode.

For new functionality: write the failing test against the desired behavior first, in GUI mode. The engine's interactive runner gives you a fast TDD loop because `GuiFunc` is your normal UI submission — you can reason about both at once.

## See also

- [bootstrap.md](bootstrap.md) — initial project + main loop scaffold; the test build typically replaces `main.cpp` with the test skeleton.
- [pitfalls-catalog.md](pitfalls-catalog.md) — when a test reveals a bug in your UI code, the canonical pitfalls live here.
- [changelog-1.92.x.md](changelog-1.92.x.md) — the engine's API tracks the upstream version closely; check for breakages when bumping.
