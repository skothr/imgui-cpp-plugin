# Test prompt intent

The files in this directory are written as **realistic user messages** — first-person, casual, no meta-language about the skill. The whole point of writing them this way is to test whether the `imgui-cpp:imgui-cpp-development` skill applies its conventions on its own, **without the user having to spell them out**.

This document captures what each prompt is actually testing — the conventions the skill should reach for unprompted, the reference docs it should route to, and the anti-patterns we'd flag as a failure even if the produced code "works."

If a prompt body and this intent doc disagree (the prompt mentions an idiom by name), the prompt is too leaky and should be tightened.

## Style — diagnostic prompts are symptoms-first

For "broken code, fix it" prompts (04, 05, 06): **the prompt body must NOT include the broken code.** A real user mid-debug describes the symptom and what they've already tried, not a surgically minimal reproducer. Pasting the code hands the answer to the skill — the skill then just needs to spot the bug in 6 lines, which is a much easier task than what we actually need to evaluate (does the skill reconstruct the likely bad pattern from the symptom alone?).

The diagnostic-prompt rubric is therefore harder:

1. **Symptom interpretation** — does the skill correctly map a symptom ("buttons all delete tab[0]" / "child panel grows 1px per frame" / "popup flashes for one frame") to the canonical bug pattern that produces it?
2. **Pattern reconstruction** — does the skill describe the likely bad code structure ("you're probably calling `ImGui::Button(\"Delete\")` inside a loop without `PushID`") instead of asking the user to paste their code?
3. **Fix demonstration** — does the skill show corrected code matching the reconstructed pattern, even though the user didn't paste theirs?

A skill that says "please paste your code" on a diagnostic prompt is a partial pass at best — real users get faster help when the skill recognizes the symptom and reconstructs.

## Cross-prompt expectations (apply to all prompts)

These are the skill defaults that should show up unprompted in every relevant response — captured here once instead of repeated below.

- **Modern C++23** as the language standard in any CMake scaffold (not C++17, not C++20). Set on the project's compile target, not just CMakeLists' `cmake_minimum_required`.
- **Pin Dear ImGui to `v1.92.7-docking`** in any FetchContent declaration; pin GLFW to `3.4`. No floating refs.
- **`set(CMAKE_EXPORT_COMPILE_COMMANDS ON)`** in any generated CMakeLists so clangd / LSP works.
- **RAII scope guards from `assets/imscoped.hpp`** for every Begin/End and Push/Pop pair in produced C++ code. The skill should COPY that file into the project (e.g. as `src/imscoped.hpp`) rather than retyping it inline.
- **`std::print` / `std::println`** for any console output (`<print>` is C++23). Not `printf`. Not `std::cout`.
- **Skip rendering when the GLFW window is iconified** in any main-loop scaffold (the canonical idiom from `assets/main_glfw_opengl3.cpp.template`).
- **Locate-imgui first** on any prompt that mentions an existing project. For greenfield prompts (output dir doesn't exist yet) the locate step is moot.
- **`std::expected<T, GfxError>`** at API boundaries for fallible resource operations (texture load, shader compile, swapchain init), where applicable.
- **No internal API misuse** — public ImGui:: only unless a prompt is explicitly about internals (e.g., 08-knob-widget).

A response that produces working code but ignores these defaults is a partial pass at best.

## Per-prompt intent

### 01 — hello-world-glfw

User intent: scaffold a fresh CMake-based ImGui project on Linux that renders the demo window.

**Skill should reach for, unprompted:**

- Cross-prompt defaults above (esp. C++23 + v1.92.7-docking + imscoped.hpp + iconify-skip).
- `ImGui::ShowDemoWindow(&open)` to satisfy the user's "browse what's available" goal.
- A single Begin/End or two for a tiny status panel; demo window does the heavy lifting.

**Should route to:** `references/bootstrap.md`, `references/backends/opengl3-glfw.md`, `references/frame-loop.md`.

**Anti-patterns to flag:**

- Pinning to ImGui `v1.91.x` or older (training-memory leak; the v1.92 font system rework matters).
- C++17 default in CMakeLists.
- `printf` for any diagnostic output.
- Hand-typed `imscoped.hpp` instead of copying the bundled asset.
- Missing iconify-skip in the main loop (causes 100% CPU when minimized).

### 02 — tools-panel

User intent: a small single-window tools panel with sliders, checkboxes, color edit, and an "Apply" button that prints state to console.

**Skill should reach for, unprompted:**

- Cross-prompt defaults.
- `ImScoped::Window` for the panel; `ImScoped::ID` if grouping requires loop-style submission.
- `BeginGroup` / `EndGroup` (or `ImScoped::Group`) for the labeled sections.
- `ImScoped::StyleColor` for the locally-overridden "Demo text" color (so the override is automatically scoped, not leaking).
- `std::println` (or `std::print`) for the Apply button's state dump.

**Should route to:** `references/bootstrap.md`, `references/widget-recipes.md`, `references/styling-fonts-dpi.md` (for the style override).

**Anti-patterns to flag:**

- Manual `PushStyleColor` / `PopStyleColor` without RAII (style-stack mismatches will assert).
- `printf` in the Apply handler.
- Hand-rolling the FOV slider as `DragFloat` without bounds (prompt asked for a slider with a 30..120 range).

### 03 — sortable-file-table

User intent: a sortable, hover-highlighted, single-select file table with row striping. Should scale to thousands of entries later.

**Skill should reach for, unprompted:**

- Cross-prompt defaults.
- `ImGui::BeginTable` with flags: `Sortable | RowBg | Hideable | Reorderable | ScrollY` (or a sensible subset).
- `ImScoped::Table` (RAII, encodes the conditional-end rule).
- `TableSetupColumn` for each column with appropriate width policy (Size column right-aligned + fixed width).
- `TableHeadersRow` for the click-to-sort behavior.
- `TableGetSortSpecs` + `SpecsDirty` pattern for re-sorting on click.
- `Selectable` with `ImGuiSelectableFlags_SpanAllColumns` for whole-row selection.
- Mention `ImGuiListClipper` for virtualization at scale (the user said "should still feel responsive if the list grew to a few thousand entries" — this is the canonical answer).
- `ImScoped::ID` per row keyed on the entry, for stable selection IDs.

**Should route to:** `references/tables.md`, `references/widget-recipes.md`.

**Anti-patterns to flag:**

- Using deprecated `Columns` API instead of `BeginTable` (the skill should know `Columns` is legacy).
- Skipping `SpanAllColumns` so click only registers in the column clicked.
- No mention of clipper at all (the user explicitly hinted at scale).
- Per-row `PushID(i)` (index-based) when the entries have stable identity available.

### 04 — debug-delete-button (★ pitfall diagnosis)

User intent: figure out why every Delete button deletes tab #0.

**Skill should reach for, unprompted:**

- Run `locate-imgui` (or note it's a diagnostic-only response so skips that step).
- Identify the canonical ID-stack collision: every `ImGui::Button("Delete")` hashes to the same widget ID under the parent window, so they all "fire" against the first iteration's button.
- Cite the upstream FAQ entry / `imgui_demo.cpp`'s ID Stack Tool by `path:line`.
- Show the corrected code using `ImScoped::ID{&tab}` (object-identity, survives reorder).
- Mention defer-deletion-after-loop OR `break` after `delete_tab`, since mutating `tabs` mid-iteration with `auto& tab` invalidates the reference.
- Mention the ID Stack Tool (in Metrics / Debug Tools window) as the navigation answer for "next time".
- Mention `"Delete##<unique>"` and `PushID(int)` as alternatives so the user understands the option space.

**Should route to:** `references/id-stack.md`, optionally `references/pitfalls-catalog.md`.

**Anti-patterns to flag:**

- Recommending raw `PushID(i)` / index-based without explaining why object identity is preferred.
- Missing the secondary mid-iteration-mutation bug.
- No upstream citation.

### 05 — debug-child-grows (★ user-flagged pain area)

User intent: figure out why a child window grows ~1px per frame even though content doesn't change.

**Skill should reach for, unprompted:**

- Identify the AutoResizeY + size-feedback loop: the child measures content height, the parent allocates that height, the child re-measures with the new available space, etc. Padding / borders contribute to the per-frame delta.
- Cite `imgui.h:457` (the upstream warning that combining `AutoResizeX` + `AutoResizeY` is unrecommended) and explain why the user's case (only AutoResizeY) hits the same family of bug.
- Present 3 fixes with explicit tradeoffs:
  1. Pin a fixed height (`ImVec2(0, k_h)`).
  2. Cap growth via `SetNextWindowSizeConstraints`.
  3. Make the child user-resizable (`ImGuiChildFlags_ResizeY`).
- Recommend which one to pick for an editor specifically (typically #1 or #3).

**Should route to:** `references/layout-and-sizing.md`, `references/pitfalls-catalog.md`.

**Anti-patterns to flag:**

- "Pass `ImVec2(0, 1)`" or "use `FLT_MIN`" without the actual upstream-cited explanation (the no-skill baseline was confidently wrong about this — see `.eval-workspace/iteration-1/comparison.md`).
- Suggesting only one fix without naming the others.
- Missing the upstream citation.

### 06 — debug-popup-closes

User intent: figure out why their confirmation modal opens and immediately closes.

**Skill should reach for, unprompted:**

- Identify the `ImGui::OpenPopup("confirm-delete")` being called every frame outside any conditional → the popup re-arms continuously, but the close trigger from the previous frame still fires, producing the 1-frame flash.
- Show the canonical fix: only call `OpenPopup` inside the button's click handler.
- Use `ImScoped::PopupModal` in the corrected code.
- Note that `BeginPopupModal` is conditional-end (only call `EndPopup` when it returns true).

**Should route to:** `references/modals-and-popups.md`.

**Anti-patterns to flag:**

- Treating the bug as a render-loop ordering issue (it's not — it's the popup-trigger pattern).
- Missing the conditional-end guidance.

### 07 — docking-editor-layout

User intent: a 4-panel docked editor layout (top menubar, left/center/right panels) that builds itself on first run and respects user adjustments after.

**Skill should reach for, unprompted:**

- Cross-prompt defaults.
- `ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable` in IO config.
- `DockSpaceOverViewport` (full-window) OR a custom fullscreen window with `DockSpace` inside.
- DockBuilder API (`DockBuilderRemoveNode` / `DockBuilderAddNode` / `DockBuilderSplitNode` / `DockBuilderDockWindow` / `DockBuilderFinish`) wrapped in `static bool first_frame = true; if (first_frame) { … first_frame = false; }` so the layout only initializes once.
- The "save/restore current GL context around `UpdatePlatformWindows` + `RenderPlatformWindowsDefault`" idiom for multi-viewport.
- Style adjustment for viewports (`WindowRounding = 0`, opaque `WindowBg.w = 1.0`).

**Should route to:** `references/docking-and-viewports.md`, `references/backends/opengl3-glfw.md`, `references/frame-loop.md`.

**Anti-patterns to flag:**

- Rebuilding the dock layout every frame (overrides user adjustments).
- Forgetting the multi-viewport context save/restore.
- Forgetting the style adjustments → translucent edges on popped-out windows.
- Hand-typed dock IDs instead of the DockBuilder API.

### 08 — knob-widget

User intent: a custom rotary-knob widget for an audio plugin, with a small demo of three knobs.

**Skill should reach for, unprompted:**

- The custom-widget item protocol: `PushID` → `ItemSize` → `ItemAdd` → `ButtonBehavior` → draw via `ImDrawList`. Skipping any of these breaks layout, hit-testing, or framework occlusion.
- `IsItemHovered` / `IsItemActive` from the framework, NOT manual hit-testing.
- `ImGui::SetTooltip("%.3f", *v)` (or `BeginItemTooltip`) for the precise-value tooltip on hover.
- `GetIO().MouseDelta.y` (or equivalent) for vertical drag mapping.
- `ImDrawList` API: `AddCircle`, `AddLine` (for the tick mark), `PathArcTo` if drawing the arc.
- A standalone reusable function with the implied signature.
- Cross-prompt defaults for the demo `main.cpp`.

**Should route to:** `references/custom-widgets.md`, `references/widget-recipes.md` (for the tooltip recipe).

**Anti-patterns to flag:**

- Manual `IsMouseHoveringRect` instead of `IsItemHovered` (loses popup/menu occlusion).
- Skipping `ItemAdd` (the next widget will overlap and the knob can't be focused).
- Drawing primitives before `ItemAdd` (clipping won't apply).
- Using deprecated `ImGuiNavInput_*` if the response touches keyboard nav at all.

## Adding intent for a new prompt

When adding a new prompt:

1. Write the prompt body in first-person, mention only what a real user would mention. No skill-internal idiom names, no `ImScoped::*`, no `references/<doc>.md`, no `assets/<file>`.
2. Add a corresponding section to this file. Each section should at minimum cover: the conventions the skill should adopt unprompted, the references it should route to, and the anti-patterns we'd flag.
3. Update `tests/README.md`'s prompt table.

## Failure-mode language

When a test session misbehaves, prefer reporting it as one of these (helps the skill-creator iteration loop pattern-match later):

- **Trigger miss** — the skill didn't activate from prompt phrasing alone.
- **Routing miss** — skill loaded the wrong reference doc, or loaded too many siblings.
- **Idiom miss** — output didn't adopt the cross-prompt defaults.
- **Diagnosis miss** — debug prompt got a plausible-but-wrong root cause.
- **Citation miss** — claim made without an upstream `path:line` cite where one was warranted.
- **Hallucinated API** — output referenced a non-existent ImGui function or flag.
