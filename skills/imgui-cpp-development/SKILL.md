---
name: imgui-cpp-development
description: Use when working with Dear ImGui in C++ (v1.92.x docking branch) — bootstrapping new ImGui apps, navigating or extending existing ImGui codebases, building custom widgets via DrawList/ItemAdd/ButtonBehavior, doing layout/sizing/styling/font/DPI work, or diagnosing Begin/End mismatches, ID-stack collisions, scroll-window or child-frame growth bugs, and other ImGui pitfalls. Recommends modern C++23 idioms; ships drop-in RAII scope guards (imscoped.hpp), CMake + main.cpp scaffolding, and an imgui_test_engine harness. Trigger whenever the user mentions Dear ImGui, ImGui::Begin/End/Window/Child/Table/DockSpace/Popup, ImGuiIO, the docking branch, or the OpenGL3+GLFW backend — even if ImGui is not said explicitly when the context makes it clear (immediate-mode UI in C++, dear_imgui or third_party/imgui present, my-widget-keeps-growing or all-my-buttons-do-the-same-thing kinds of bugs).
---

# Dear ImGui (C++) development

This skill helps you work with [Dear ImGui](https://github.com/ocornut/imgui) in modern C++. It targets the **v1.92.x docking branch**. Each topic lives in its own reference under `references/`; load only the ones the current task actually needs.

## First action when the skill triggers

Before answering or recommending code, **locate Dear ImGui in the user's project** so subsequent guidance is grounded in the version actually present. The skill ships a script:

```bash
bash skills/imgui-cpp-development/scripts/locate-imgui.sh [project-root]
# or, from inside a project:
bash $CLAUDE_PLUGIN_ROOT/skills/imgui-cpp-development/scripts/locate-imgui.sh
```

It scans common directory roots, parses `IMGUI_VERSION` / `IMGUI_HAS_DOCK` from `imgui.h`, and reports the version + branch. Read [references/locate-imgui.md](references/locate-imgui.md) for the full procedure (what to do when ImGui is missing, how to add it via CMake/FetchContent / git submodule / manual download, how to upgrade safely, how to verify `compile_commands.json` is wired up so LSP navigation works).

Skip this step only if the user is asking a purely conceptual question with no project tree (e.g. "explain ImGui's frame loop" without code in scope).

## Default conventions this skill recommends

Apply these unless the user has explicitly opted out or the surrounding code follows a different convention (in which case follow theirs).

1. **RAII scope guards for every paired call.** Use `imscoped.hpp` (bundled at `assets/imscoped.hpp`):
   ```cpp
   if (auto w = ImScoped::Window("Tools")) {
       ImScoped::ID id{idx};
       ImScoped::StyleColor color{ImGuiCol_Text, IM_COL32(255, 200, 0, 255)};
       ImGui::Text("...");
   }   // ~Window calls End(); ~ID calls PopID(); ~StyleColor calls PopStyleColor(1)
   ```
   Why: ImGui's Begin*/End* and Push*/Pop* discipline is the single largest source of bugs in real codebases. RAII makes the pairing impossible to miss across early returns and exceptions. See [references/raii-scope-guards.md](references/raii-scope-guards.md).

2. **Begin/End rules — the one ImGui asymmetry to remember.** `Begin` and `BeginChild` are the **only** Begin*X* calls whose `End*X*` runs **regardless** of return. Every other paired API (`BeginMenu`, `BeginPopup`, `BeginCombo`, `BeginListBox`, `BeginTooltip`, `BeginPopupModal`, `BeginTable`, `BeginTabBar`, `BeginTabItem`, `BeginDragDropSource`, `BeginDragDropTarget`, `BeginItemTooltip`) requires `End*X*` **only when Begin*X* returned true**. The `imscoped.hpp` guards encode this difference; if you write the calls manually, get this right (it's a real assert when you don't).

3. **`std::expected<T, GfxError>` at API boundaries** for fallible resource ops (texture load, shader compile, swapchain init). ImGui itself doesn't use exceptions; mirror that in your wrappers.

4. **Diagnostics: `std::fprintf`/`std::printf` is the safe default; promote to `std::print`/`std::println` only when the toolchain supports it.** `<print>` and the `<format>` it builds on require **libstdc++-14 or libc++-18+** AND a compiler whose ranges-constraint checking is compatible with that stdlib's `<bits/unicode.h>`. clang 17 against libstdc++-14 currently fails to compile `<format>` (LLVM-61763 family). clang 17 against libstdc++-11/12 (Ubuntu 22.04 default) lacks `<print>` entirely. **Default the template / generated scaffolds to `<cstdio>` `std::fprintf` / `std::printf` so the bootstrap builds on every reasonable Linux + clang combination from clang 14 onward.** printf format strings are type-checked at compile time via `__attribute__((format))`, cost nothing in readability for diagnostics, and ship with C++98. Recommend `std::println` for the user's own code only after confirming their stdlib + compiler combination supports it; mention the deployment cost.

5. **Strict ID-stack hygiene.** `PushID(ptr)` for objects, `PushID(int)` for stable indices, `"##invisible-suffix"` to disambiguate same-label widgets without changing display, `"###override"` to keep state across label changes. Never rely on auto-generated labels alone in loops. See [references/id-stack.md](references/id-stack.md).

6. **No modules, no coroutines, no aggressive ranges-based widget views in v1.** They fight ImGui's API surface and add maintenance load without commensurate value.

7. **Third-party headers are SYSTEM includes.** When generating a CMakeLists that pulls in ImGui (or any other vendored C/C++ library) via FetchContent / submodule / system package, expose its include directories with `target_include_directories(<lib> SYSTEM PUBLIC ...)`. ImGui's headers contain old-style casts, `memset((void*)this, ...)` zero-init tricks, and other patterns that trip every strict warning the user's own code likely turns on (`-Wold-style-cast`, `-Wconversion`, `-Wsign-conversion`, `-Wcast-align`, `-Wdouble-promotion`). The `SYSTEM` keyword translates to `-isystem` on GCC/Clang and `/external:I` on MSVC; the compiler silences warnings from those paths while leaving strict flags fully in effect for the user's own translation units. Without it, a single `#include <imgui.h>` produces 50+ lines of `-Wold-style-cast` noise per .cpp file — ugly, and worse, drowns out warnings the user actually wants to see in their own code.

8. **Cite line numbers only from references you've actually loaded this session.** When citing `imgui.cpp:N`, `imgui.h:N`, `imgui_widgets.cpp:N`, or `FAQ.md:N`, the line number must come from the text of a `references/*.md` doc you read this session — not from training memory. The vendored source is stable, so the references' line numbers are authoritative; copy them verbatim. If your routing decision skipped a reference but you want a precise citation anyway, **load it** — a partial Read of Tier 1 is cheap insurance against drift. If a partial load doesn't include the line you want to cite, prefer naming the function or section without a number (e.g. "`ImGuiWindow::GetID` in `imgui.cpp`'s `[SECTION] ID STACK` block") over inventing a line from memory. A wrong `imgui.cpp:N` costs the user's trust the next time they try to look one up; a vague but accurate citation costs nothing.

## Routing — load the right reference for the task

The references are designed for **independent loading** — load exactly the one(s) the task needs and nothing else.

| If the task is about… | Load (default tier) |
|---|---|
| Setting up a new ImGui project (build, main loop) | [references/bootstrap.md](references/bootstrap.md) — short, load whole |
| Detecting / installing / pinning ImGui in a project | [references/locate-imgui.md](references/locate-imgui.md) — short, load whole |
| The per-frame lifecycle (NewFrame / Render / multi-viewport) | [references/frame-loop.md](references/frame-loop.md) — short, load whole |
| ID stack rules, collisions, debugging | [references/id-stack.md](references/id-stack.md) — Tier 1 only is enough for ~80% of queries |
| Window / Child sizing, scroll behavior, child-frame growth bugs | [references/layout-and-sizing.md](references/layout-and-sizing.md) — Tier 1 covers "child keeps growing" + sizing-mode cheat sheet |
| Style stack, fonts, font atlas, DPI | [references/styling-fonts-dpi.md](references/styling-fonts-dpi.md) — Tier 1 covers all common recipes |
| Docking layouts, multi-viewport | [references/docking-and-viewports.md](references/docking-and-viewports.md) — Tier 1 covers config + dockspace + viewport setup |
| Tables (BeginTable / column setup / sorting) | [references/tables.md](references/tables.md) — TLDR at top has the canonical recipe |
| Modals, popups, context menus | [references/modals-and-popups.md](references/modals-and-popups.md) — Tier 1 covers canonical modal + the "OpenPopup every frame" bug |
| Day-to-day widget recipes (drag-drop, color, tooltip, combo, tree, disabled, focus) | [references/widget-recipes.md](references/widget-recipes.md) — symptom-to-recipe table at top, then load just one recipe |
| Authoring custom widgets, DrawList primitives, hit-testing | [references/custom-widgets.md](references/custom-widgets.md) — TLDR has the 6-step skeleton |
| Cross-cutting pitfall index → which deep-dive | [references/pitfalls-catalog.md](references/pitfalls-catalog.md) — already a card-per-row index |
| What changed in v1.92.x (upgrade planning) | [references/changelog-1.92.x.md](references/changelog-1.92.x.md) — section per version |
| Headless / interactive testing of ImGui apps | [references/testing.md](references/testing.md) — Tier 1 covers anatomy + registration + API surface |
| Navigating ImGui's monofiles via clangd / LSP | [references/lsp-navigation.md](references/lsp-navigation.md) — short, load whole |
| Backend: OpenGL 3 + GLFW (init, frame, shutdown, pitfalls) | [references/backends/opengl3-glfw.md](references/backends/opengl3-glfw.md) — Tier 1 covers init + frame + multi-viewport + pitfalls |

Other backends (Vulkan, DX11, DX12, Metal, WebGPU, SDL3 platform) and other build systems (Meson, Bazel, Premake, raw Makefiles) are tracked as feature requests — not yet first-class in this skill. If the user asks about one of those, say so explicitly and fall back to general guidance + a pointer to the upstream backend file.

**Read references partially when you can.** Each reference uses one of three navigational shapes; pick the right partial-load strategy for each:

1. **Three-tier topical reference** (`id-stack`, `layout-and-sizing`, `styling-fonts-dpi`, `docking-and-viewports`, `modals-and-popups`, `testing`, `backends/opengl3-glfw`). Opens with a "Tier guidance" line giving exact line ranges. Tier 1 covers the 80% case (canonical recipes + diagnosis); Tier 2 is mechanism/catalog; Tier 3 is appendix. Default: load Tier 1 only.
2. **Q&A / API catalog** (`widget-recipes`, `pitfalls-catalog`, `tables`, `custom-widgets`). Self-contained sections; either a symptom-to-recipe table or a TLDR sits near the top. Default: read the lookup table or TLDR, then jump to one section via the Quick navigation block.
3. **Linear walkthroughs** (`bootstrap`, `locate-imgui`, `lsp-navigation`, `frame-loop`, `raii-scope-guards`, `changelog-1.92.x`). Short enough to load whole, or read in source order.

Whichever shape, every reference > ~200 lines opens with a `<!-- QUICK_NAV_BEGIN -->` block listing every section and its current line range — use `Read offset=<L> limit=<N>` against that.

Examples:

- "all my buttons do the same thing" → `id-stack.md` Tier 1 only (`Read offset=1 limit=38`).
- "child window grows every frame" → `layout-and-sizing.md` Tier 1 (the §1 fix recipe); load Tier 2 mechanism only if user asks why.
- "x glyph isn't rendering" → `styling-fonts-dpi.md` Tier 1 §6 (Non-ASCII characters in widget labels) via partial read.
- "how do I set up GLFW + OpenGL?" → `backends/opengl3-glfw.md` Tier 1 (init + per-frame + multi-viewport + pitfalls) is the entire answer.
- "how do I make a tooltip?" → `widget-recipes.md` symptom table → §3 Tooltips, partial read.

Loading a reference whole is a deliberate signal that you expect to consult multiple sections. Default to partial.

If you've edited a reference and the section line ranges have shifted, refresh the Quick navigation blocks by running:

```bash
python3 skills/imgui-cpp-development/scripts/refresh-quicknav.py
```

## LSP-driven navigation of ImGui's source

Dear ImGui's source is itself the canonical documentation: **read it directly when in doubt.** The fastest way is via the `LSP` tool against clangd:

| Goal | LSP operation |
|---|---|
| Find `ImGui::Begin` (or any symbol) | `LSP workspaceSymbol "ImGui::Begin"` |
| Get a TOC of `imgui.h` or `imgui.cpp` | `LSP documentSymbol` on the file |
| Jump to definition once parked on a token | `LSP goToDefinition` |
| Find every call site of a function | `LSP findReferences` |
| Read inline doc / type info | `LSP hover` |

This requires `compile_commands.json` to exist for the user's project (or for the vendored ImGui copy). If LSP fails with "no compile commands," run `scripts/ensure-compile-commands.sh` and follow the project-appropriate snippet it prints. See [references/lsp-navigation.md](references/lsp-navigation.md) for full recipes.

## Common quick recipes (inline; deeper material is in references)

These are short enough to keep here so trivial questions don't require loading a reference.

**Show ImGui's interactive demo (the canonical "everything is in here" reference)**

```cpp
ImGui::ShowDemoWindow();   // submit once per frame
```
The demo source is `imgui_demo.cpp` in the user's vendored copy — **read it as the primary reference**. Almost every "how do I X" question is answered by a section there.

**Standard frame**

```cpp
ImGui_ImplOpenGL3_NewFrame();   // backend renderer
ImGui_ImplGlfw_NewFrame();      // backend platform
ImGui::NewFrame();              // ImGui core
// ... user UI submission ...
ImGui::Render();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
```

**A scrollable list with stable IDs**

```cpp
if (auto child = ImScoped::Child("##list", ImVec2(0, -1), ImGuiChildFlags_Borders)) {
    for (size_t i = 0; i < items.size(); ++i) {
        ImScoped::ID id{static_cast<int>(i)};
        if (ImGui::Selectable(items[i].label.c_str())) {
            select(items[i]);
        }
    }
}
```

For anything more involved (tables, drag-drop, custom widgets, modals, etc.), load the relevant reference.

## Bundled assets

| Asset | Purpose |
|---|---|
| `assets/imscoped.hpp` | Drop-in RAII scope guards for every Begin/End and Push/Pop pair. Header-only, depends only on `<imgui.h>`. |
| `assets/main_glfw_opengl3.cpp.template` | C++23 main-loop template using imscoped.hpp; OpenGL 3 + GLFW + docking + multi-viewport + per-monitor DPI. |
| `assets/CMakeLists-glfw-opengl3.txt.template` | CMake project that fetches Dear ImGui v1.92.7-docking + GLFW 3.4 via FetchContent and builds them as a static lib. Emits `compile_commands.json`. |
| `assets/imgui_test_skeleton.cpp.template` | imgui_test_engine harness; supports both interactive and `--headless` CI modes. |

When recommending a setup, copy the relevant template to the user's project rather than typing one from memory.

**Source assets from `$CLAUDE_PLUGIN_ROOT`, never from `~/.claude/plugins/cache/...`.** Claude Code sets `$CLAUDE_PLUGIN_ROOT` to the live source root of the currently-loaded plugin — whether the plugin was loaded via `--plugin-dir`, user-scope install, project install, or marketplace cache. The canonical copy command is:

```bash
cp "$CLAUDE_PLUGIN_ROOT/skills/imgui-cpp-development/assets/imscoped.hpp" <dest>/
cp "$CLAUDE_PLUGIN_ROOT/skills/imgui-cpp-development/assets/main_glfw_opengl3.cpp.template" <dest>/src/main.cpp
cp "$CLAUDE_PLUGIN_ROOT/skills/imgui-cpp-development/assets/CMakeLists-glfw-opengl3.txt.template" <dest>/CMakeLists.txt
```

The path under `~/.claude/plugins/cache/` is a snapshot from the last marketplace install. It can be stale by hours, days, or weeks compared to the live source — and an agent that copies from it ships outdated templates to the user's project. `$CLAUDE_PLUGIN_ROOT` is the only path that's guaranteed fresh for the active session.

## What this skill is NOT for

- Generic C++ help unrelated to ImGui — defer to base Claude.
- Other immediate-mode UI libraries (Nuklear, microui). The pitfalls don't transfer.
- ImPlot, imgui-node-editor, imgui-filebrowser, etc. They're worth pointing the user at, but their internals are out of scope.
- Pixel-level rendering correctness. The skill's testing reference covers logic-level testing only; visual regression needs a separate pipeline.
