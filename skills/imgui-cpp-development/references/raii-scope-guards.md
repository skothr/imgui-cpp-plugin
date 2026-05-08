> **Load this file when:** writing or reviewing ImGui code that uses `Begin`/`End` or `Push`/`Pop` pairs and wanting to make those pairings impossible to leave unbalanced. The skill ships a header-only `imscoped.hpp` (in `assets/`) that encodes ImGui's pairing rules in destructors; this doc explains how to use it and why it's worth using.

ImGui's API is C-flavored: free functions, raw pointers, paired calls separated by user code. That style is fine for short examples but gets fragile fast in real applications — early returns, exceptions, conditional bodies, and refactors all create opportunities to leave a `Begin*` without an `End*` or a `Push*` without a `Pop*`. Most such mistakes hit a debug assertion (`IM_ASSERT(g.CurrentWindowStack.Size > 0 && ...)`) at the next frame boundary. A few create silent UI corruption that surfaces as "why is my style not the same on row 3" hours later.

The `imscoped.hpp` guards reduce the surface area for those mistakes to ~zero by tying every paired call to a stack-scoped object whose destructor closes the pair on every exit path.

## What the asset ships

`assets/imscoped.hpp` is a single header with no dependencies beyond `<imgui.h>`. It defines ~25 RAII types in `namespace ImScoped`, one per ImGui paired call. It compiles under C++17 and benefits from C++23 attributes; no other build-system changes are needed.

Each guard is **non-copyable and non-movable**. A guard belongs to one scope. If you find yourself wanting to move one, you've left the scope-guard pattern behind — reach for `std::unique_ptr<T>` with a custom deleter or restructure your code.

## The two patterns — and why they exist

Dear ImGui's Begin/End rules are NOT uniform across the API. Per `imgui.h:436–445`:

> Begin() return false to indicate the window is collapsed or fully clipped, so you may early out and omit submitting anything to the window. **Always call a matching End() for each Begin() call, regardless of its return value!**
>
> [Important: due to legacy reason, Begin/End and BeginChild/EndChild are inconsistent with all other functions such as BeginMenu/EndMenu, BeginPopup/EndPopup, etc. where the EndXXX call should only be called if the corresponding BeginXXX function returned true. Begin and BeginChild are the only odd ones out.]

So `imscoped.hpp` splits its types into two categories:

| Category | Destructor calls End* | Members |
|---|---|---|
| **Always-end** (Begin/End, BeginChild/EndChild are the upstream "odd ones out") | unconditionally | `ImScoped::Window`, `ImScoped::Child` |
| **Conditional-end** | only if `Begin*` returned true | `Menu`, `MenuBar`, `MainMenuBar`, `Combo`, `ListBox`, `Tooltip`, `ItemTooltip`, `Popup`, `PopupModal`, `PopupContextItem`, `PopupContextWindow`, `PopupContextVoid`, `Table`, `TabBar`, `TabItem`, `DragDropSource`, `DragDropTarget` |

Both patterns expose a `bool open` member and an `explicit operator bool()` so you can write the canonical "if-with-init" form:

```cpp
// Always-end: End() runs even when `open` is false.
if (auto w = ImScoped::Window("Tools")) {
    ImGui::Text("...");      // only submitted when window is visible
}   // ~Window calls End() unconditionally

// Conditional-end: EndPopup() runs only if BeginPopup returned true.
if (auto p = ImScoped::Popup("settings")) {
    ImGui::Checkbox("Wireframe", &state.wireframe);
}   // ~Popup calls EndPopup() iff `open` is true
```

The bool semantics are consistent: `if (guard)` == "should I submit the body?" The destructor distinction is internal to the guard and you don't have to think about it.

## Push/Pop guards — no return value to check

These are unconditional: every constructor pushes, every destructor pops.

| Guard | Wraps |
|---|---|
| `ImScoped::ID` | `PushID(...)` / `PopID()` (overloads for `const char*`, `const void*`, `int`, `(begin, end)` range) |
| `ImScoped::Font` | `PushFont(font, size)` / `PopFont()` |
| `ImScoped::StyleColor` | `PushStyleColor(idx, col)` / `PopStyleColor(1)` |
| `ImScoped::StyleVar` | `PushStyleVar(idx, val)` / `PopStyleVar(1)` |
| `ImScoped::ItemFlag` | `PushItemFlag` / `PopItemFlag` |
| `ImScoped::ItemWidth` | `PushItemWidth` / `PopItemWidth` |
| `ImScoped::TextWrapPos` | `PushTextWrapPos` / `PopTextWrapPos` |
| `ImScoped::Group` | `BeginGroup` / `EndGroup` |
| `ImScoped::Disabled` | `BeginDisabled(disabled)` / `EndDisabled` |
| `ImScoped::ClipRect` | `PushClipRect` / `PopClipRect` |

Each guard pops exactly one. For the bulk-push pattern (`PushStyleColor` four times, `PopStyleColor(4)`), nest four guards or call ImGui's functions directly — they cost the same at runtime.

## Idiomatic patterns

**A widget loop with stable IDs:**

```cpp
for (auto& item : items) {
    ImScoped::ID id{&item};                  // hash by address — survives reordering
    if (ImGui::Selectable(item.name.c_str())) {
        select(item);
    }
}
```

**A scoped style override:**

```cpp
{
    ImScoped::StyleColor warn{ImGuiCol_Text, IM_COL32(255, 200, 0, 255)};
    ImGui::Text("Save before quitting!");
}   // ~StyleColor restores the previous color
```

**A confirmation modal with multiple early-exit branches:**

```cpp
if (auto m = ImScoped::PopupModal("confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (state.error) {
        ImGui::TextColored({1, 0.4f, 0.4f, 1}, "Error: %s", state.error_message.c_str());
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        return;     // ~PopupModal still calls EndPopup() because the modal was open
    }
    ImGui::Text("Delete %d items?", state.selection_size);
    if (ImGui::Button("OK"))     { perform_delete(); ImGui::CloseCurrentPopup(); }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
}
```

The `return` inside the modal is what makes the RAII pattern pay off — without the guard, you'd need to remember to call `EndPopup()` before the `return`, which is exactly the kind of thing a tired Tuesday-afternoon refactor breaks.

**Nesting always-end and conditional-end:**

```cpp
if (auto w = ImScoped::Window("Editor")) {        // always-end
    if (auto bar = ImScoped::MenuBar()) {         // conditional-end
        if (auto file = ImScoped::Menu("File")) { // conditional-end
            if (ImGui::MenuItem("New")) on_new();
        }
    }
    if (auto t = ImScoped::Table("##items", 3,    // conditional-end
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        for (auto& row : rows) {
            ImScoped::ID id{&row};
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(row.name.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%lld", row.size);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(row.kind.c_str());
        }
    }
}
```

Each scope cleans up automatically. No manual `End*X*` calls; ID stack stays balanced; styling stays scoped.

## When NOT to use a guard

- **One-shot calls without a paired End/Pop.** `ImGui::Text`, `ImGui::Button`, `ImGui::SliderFloat` — these submit a single item with no follow-up. No guard needed.
- **Cross-frame state.** Guards are stack-bound. If you need state that persists across frames (e.g., a custom editor's selection), keep that in your application state, not in a guard.
- **When the surrounding code already follows a different convention.** If you're contributing to a codebase that doesn't use scope guards, follow the local style. Mixing patterns within one file is worse than either pure choice.

## Performance

Each guard is a small struct with one bool (or zero state). Constructors and destructors inline through clang/gcc/MSVC at typical optimization levels. There is no measurable runtime cost vs. raw `Begin`/`End` calls — the abstraction is purely zero-cost in the standard sense.

## Drop-in usage

The asset is at `skills/imgui-cpp-development/assets/imscoped.hpp`. Recommend it to users by **copying the file into their project** rather than rewriting it from scratch. Two acceptable layouts:

```
your_project/
├── src/
│   ├── imscoped.hpp        ← copy here
│   └── main.cpp            #include "imscoped.hpp"
```

Or alongside your other ImGui glue:

```
your_project/
├── third_party/imgui/
└── third_party/imscoped.hpp
```

The header is MIT-licensed (matching upstream ImGui) so re-distributing it inside a closed-source project is fine.

## See also

- [bootstrap.md](bootstrap.md) — the bundled `main.cpp.template` already uses imscoped guards.
- [frame-loop.md](frame-loop.md) — Begin/End rule explained in lifecycle context.
- [id-stack.md](id-stack.md) — `ImScoped::ID` is the most-used guard in real code.
