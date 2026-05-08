> **Load this file when:** answering "where is `ImGui::Foo` defined", "show me everywhere we call X", "what does this function do" — anything that wants to navigate Dear ImGui's monofiles authoritatively. ImGui is structured as ~30k-line `.cpp` files; grep is workable but clangd-driven LSP is dramatically more reliable for symbol-level work.

Dear ImGui's source code is, by upstream's own description, the canonical documentation. The leading comment block in `imgui.cpp` is several hundred lines long and indexes every section of the file. Reading the source IS the documentation pass. Two practical access patterns:

1. **LSP via clangd** — semantic, symbol-aware, fast on warm files. Use this whenever `compile_commands.json` is set up.
2. **Plain Read + Grep** — fallback when LSP isn't available, or for searching free-text comments rather than symbol identifiers.

This document is mostly about (1) because it's underused and powerful for ImGui's monofile structure.

## Prerequisite: `compile_commands.json`

clangd needs to know how each translation unit is compiled — include paths, defines, language standard. That information lives in `compile_commands.json`. Without it, clangd falls back to heuristics that miss most of ImGui's API.

Use the bundled script to verify and emit a project-appropriate snippet:

```bash
bash skills/imgui-cpp-development/scripts/ensure-compile-commands.sh [project-root]
```

It detects the build system (CMake / Meson / Bazel / Premake / Makefile) and prints the recommended commands. For CMake the answer is one line:

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

…then re-configure (`cmake -S . -B build`) and either symlink `build/compile_commands.json` to the project root or configure clangd to look in `build/`. clangd's default search heuristic finds either.

If the user is editing inside `vendor/imgui` itself (i.e., this plugin's dev mode), see the repo-root `CLAUDE.md` for setup. For users installing this plugin: their own project's `compile_commands.json` already covers ImGui's headers since they include them.

## The four LSP operations that matter for ImGui

| Operation | When to reach for it | Example |
|---|---|---|
| `LSP workspaceSymbol` | Find a symbol by name across the whole workspace. The fastest "where is `Foo`?" tool. | `LSP workspaceSymbol "ImGui::Begin"` returns the `Begin` definition site immediately. |
| `LSP documentSymbol` | Get a complete table of contents for a single file. Useful for orienting in 24k-line `imgui.cpp`. | `LSP documentSymbol` on `imgui.h` lists every public-API declaration. |
| `LSP goToDefinition` | Jump to the definition of the symbol at a specific position. | After parking on `BeginPopup` in `imgui_demo.cpp:N`, this lands you in `imgui_widgets.cpp` where `BeginPopup` is implemented. |
| `LSP findReferences` | Find every call site / use site of a symbol. | On `ImGui::ItemAdd`, this lists every internal use across `imgui_widgets.cpp` and `imgui_tables.cpp` — the canonical "how do other widgets use this" answer. |

Two more that help less often but are worth knowing:

- `LSP hover` — inline doc + type info. ImGui's headers have rich comments adjacent to many declarations; `hover` surfaces them without a full-file Read.
- `LSP goToImplementation` — for virtual methods / interface dispatch. ImGui has very few of these; rarely useful in this codebase.

## Recipes

### "Where is `ImGui::Begin` actually implemented?"

```text
LSP workspaceSymbol "ImGui::Begin"
```

Returns the declaration in `imgui.h` and the definition in `imgui.cpp` (the implementation is several hundred lines). Pick the `imgui.cpp` entry, jump there, then `documentSymbol` if you want the surrounding section index.

### "What's the public API of imgui.h?"

```text
LSP documentSymbol on /path/to/imgui.h
```

Returns the entire table of contents. The output is large — clangd lists every typedef, enum, function, and member. For a narrower view, `documentSymbol` on a backend header (`imgui_impl_glfw.h`) gives a compact backend API surface.

### "Who calls `ItemAdd`?"

Park on the `ImGui::ItemAdd` declaration in `imgui_internal.h`, then:

```text
LSP findReferences
```

You get the full list of internal call sites in `imgui_widgets.cpp` / `imgui_tables.cpp` / `imgui.cpp`. Read 2–3 representative ones to learn how upstream uses the function — that's the most reliable way to pattern your own custom widget on the standard recipe.

### "What does `ImGuiWindowFlags_NoMove` actually mean?"

Park on `ImGuiWindowFlags_NoMove` (any usage site), then `LSP hover`. ImGui's enums have rich comments on each value; the hover content quotes them inline.

### "Which file owns `BeginPopup`?"

```text
LSP workspaceSymbol "BeginPopup"
```

Returns multiple entries: declaration in `imgui.h`, implementation in `imgui_widgets.cpp`. Jump to the implementation; the function header comment (which clangd surfaces) usually quotes the canonical lifecycle rule.

## Falling back to grep when LSP isn't available

When clangd can't be set up (sandboxed env, missing dependencies, build system not yet wired up), use `Bash(grep -rn ...)` against the user's vendored ImGui copy. Two patterns that work well:

```bash
# Find a symbol's declaration
grep -rn 'IMGUI_API.*Begin' vendor/imgui/imgui.h

# Find every "Note:" comment in imgui.cpp (often more useful than reading sequentially)
grep -n 'Note:\|NOTE:\|FIXME:\|WARNING:' vendor/imgui/imgui.cpp

# Find a section header
grep -n '\[SECTION\]' vendor/imgui/imgui.cpp
```

The `[SECTION]` markers in `imgui.cpp` and `imgui_widgets.cpp` partition the monofiles into logical chunks — that's how upstream itself navigates them.

## A note on ranges and reading large files

Reading 24k-line `imgui.cpp` cover-to-cover wastes context. The right pattern:

1. Use `documentSymbol` (or `grep '\[SECTION\]'`) to get the section list.
2. Pick the relevant section.
3. Read only that range with `Read offset=N limit=M`.

For `imgui_demo.cpp`, the file is structured as `ShowDemoWindow*` functions — one per topic (Widgets, Layout, Popups, Tables, etc.). Locate the right `ShowDemoWindowX` via `workspaceSymbol`, then read its body. The demo's body is the canonical "how do I do X" answer for almost every common task.

## See also

- [locate-imgui.md](locate-imgui.md) — find the ImGui copy whose source you'll be navigating.
- [bootstrap.md](bootstrap.md) — initial project setup including the `compile_commands.json` step.
- [pitfalls-catalog.md](pitfalls-catalog.md) — index of cross-cutting traps; many entries point at specific source lines worth reading.
