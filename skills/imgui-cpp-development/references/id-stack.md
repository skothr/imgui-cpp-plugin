# ID stack: how IDs are computed, why collisions happen, how to fix them

> **Load this file when:** debugging "all my buttons do the same thing", "my widget state resets when I scroll", or any other ID-collision symptom; or when authoring widgets in a loop / inside a custom container that needs stable identification.
>
> **Tier guidance:** Tier 1 (lines 9-38) covers the 80% case — diagnosis + canonical fix recipe — and is enough to answer almost every collision question. Tier 2 (lines 40-127) is the mechanism, the full collision-pattern catalog, the `##` / `###` label conventions, and the debugging tools. Tier 3 (lines 129-end) is the v1.92.6 hash-change upgrade hazard plus See-also links. Default: load Tier 1 only via `Read offset=1 limit=38`.

---

## Tier 1 — Quick answer: ID-collision in a loop

**Symptom:** every iteration of a loop submits widgets with the same label, and clicks / state seem to resolve to the first iteration only. `Button("Delete")` × N → only the first one ever fires; `DragFloat("My value", ...)` × N → editing one drags the first.

**Why:** ImGui identifies every widget by hashing `(window_name, …PushID values…, label)`. If the loop body has no `PushID` and the labels are identical, every iteration produces the same ID. ImGui resolves interaction to the first widget that owns the ID; subsequent submissions are duplicates.

**Fix.** Push a stable per-iteration ID before submitting widgets. With the bundled `imscoped.hpp` guards, the pop happens automatically:

```cpp
for (auto& item : items) {
    ImScoped::ID id{&item};               // hash by object identity
    if (ImGui::Selectable(item.name.c_str())) {
        on_select(item);
    }
}
```

Two properties: `&item` is the post-reorder address of the vector element, so sorting `items` doesn't invalidate per-row state; the `id` destructor runs on every iteration including `break` / throw, so the stack stays balanced without explicit `PopID`. **This is the canonical recipe** — anywhere you find yourself writing manual `PushID(i)` / `PopID()` around a Selectable, Button, or DragFloat in a loop, prefer this.

Pick the right `PushID` overload for your data:

- `ImScoped::ID id{&obj}` — pointer / object-identity. Survives reorder.
- `ImScoped::ID id{i}` — integer index. Fast but breaks on insert / delete / sort.
- `ImScoped::ID id{name}` — string. Stable but more expensive.

**Secondary bug to also catch.** If the user's loop also calls something that mutates the container they're iterating (`delete_tab(tab)` while looping `tabs`), the iterator / reference is invalid for the rest of the iteration. Defer the mutation: capture the target id in a local, do the mutation after the loop ends.

**Canonical citation:** `FAQ.md:309-341` (`Q: How can I have multiple widgets with the same label?`); broader ID-stack context at `FAQ.md:266-307` (`Q: About the ID Stack system...`); the upstream code is at `imgui.cpp:9772-9784` (`ImGuiWindow::GetID(const char*)` body).

---

## Tier 2 — Mechanism, full pattern catalog, debugging tools

### How IDs are computed

A widget's ID is a hash of its label, seeded by the *current* value at the top of the per-window ID stack. From `imgui.cpp:9772-9784`:

```cpp
ImGuiID ImGuiWindow::GetID(const char* str, const char* str_end)
{
    ImGuiID seed = IDStack.back();
    ImGuiID id = ImHashStr(str, str_end ? (str_end - str) : 0, seed);
    // ...
}
```

The seed (`IDStack.back()`) is whatever the parent context last pushed. When a window is created, its base ID stack entry is the hash of the window's name (`imgui.cpp:4671`: `IDStack.push_back(ID)` in the `ImGuiWindow` constructor, where `ID = ImHashStr(name)`). That base entry is restored at the start of each `Begin()` call (`imgui.cpp:7881-7882`).

So every widget ID is implicitly: `hash(window_name, ...PushID values..., ...implicit container seeds (e.g. TreeNode)..., widget_label)`. The "parent ID stack" is the literal stack at `ImGuiWindow::IDStack` — it includes the window's own ID at the bottom, every active `PushID`, and any implicit pushes from container widgets (`TreeNode`, `BeginChild`, popups).

Three overloads of `GetID` exist for the three common identifier shapes — string (`imgui.cpp:9772`), pointer (`imgui.cpp:9785`), integer (`imgui.cpp:9797`) — combining via `ImHashStr` / `ImHashData` against the same seed. `PushID` is the user-facing wrapper that calls `GetID` and pushes the result onto `IDStack` (`imgui.cpp:9828-9858`).

### The `##invisible-suffix` and `###override` label conventions

Two label conventions let you control ID separately from the visible label without `PushID`. Both are documented in `FAQ.md:266-389`.

**`##suffix` — same display, different ID.** Everything after `##` is part of the hash but is not rendered. Use this to disambiguate a small number of widgets that should display the same label.

```cpp
// Before — both buttons collide on hash("WindowID", "Apply"):
ImGui::Button("Apply");
ImGui::Button("Apply");   // same ID — clicking either triggers the first

// After — same display, distinct IDs:
ImGui::Button("Apply##save-button");
ImGui::Button("Apply##discard-button");
```

**`###override` — different display, same ID.** Everything *before* `###` is ignored for hashing; only what follows participates in the ID. Use this to keep state stable when the visible label changes every frame.

```cpp
// Before — label and ID are coupled, so each frame is a fresh widget,
// losing focus / drag-in-progress / open state on every counter tick:
char buf[64];
std::snprintf(buf, sizeof(buf), "Frame %d", frame_count);
ImGui::Begin(buf);

// After — display animates, ID stays hash("###frame"):
char buf[64];
std::snprintf(buf, sizeof(buf), "Frame %d###frame", frame_count);
ImGui::Begin(buf);
```

### Full collision-pattern catalog

**1. Same label in a loop.** Already covered in Tier 1 above; included here for completeness as one of four patterns. The canonical FAQ entry is at `FAQ.md:309-341`.

**2. Two windows with identical content widgets.** This is *not* a bug — windows scope IDs. `Button("OK")` inside `Begin("WindowA")` hashes to `hash("WindowA", "OK")`; the same `Button("OK")` inside `Begin("WindowB")` hashes to `hash("WindowB", "OK")`. State does not cross windows. If state seems to cross, you almost certainly have a single window being submitted twice with the same name — look for misbalanced `Begin`/`End`, or two code paths calling `Begin("Editor")` in the same frame (which makes them the same window, not two).

**3. Tree node state and child widget IDs.** `TreeNode` itself pushes onto the ID stack. So a button inside a tree node hashes as `hash(window, treenode_label, button_label)`. If you change the tree node's label (without `###`), the child widget IDs change too. Usually harmless because the tree node was collapsed; but if you're animating the tree node's label, child state will reset. Fix by giving the tree node a stable `###` ID, or by pushing your own `ImScoped::ID` *inside* the tree node so children depend on a stable seed regardless of the tree node label.

**4. Programmatic widget generation across docked layouts.** Docked windows are still distinct windows — they keep their own ID stack rooted at `hash(window_name)`. The collision risk is when you generate the same window *name* twice in different dock nodes (say, two "Properties" panels). Either give each a unique name (`"Properties##selectionA"`) or share state explicitly. If a docked window is re-parented at runtime, IDs do not change — they are tied to the window name, not the dock node.

### Begin/End scoping rule for IDs

The ID stack is per-window. `Begin("X")` resets the working stack to `[hash("X")]` for the duration of the block; `End` pops back to the previous window's stack. `PushID` and `PopID` must balance *within* the same window scope. Calling `PopID` more times than `PushID` triggers an assert (`imgui.cpp:9901-9907`):

```cpp
void ImGui::PopID()
{
    ImGuiWindow* window = GImGui->CurrentWindow;
    IM_ASSERT_USER_ERROR_RET(window->IDStack.Size > 1, "Calling PopID() too many times!");
    window->IDStack.pop_back();
}
```

The base entry (the window's own ID, pushed by `Begin` at `imgui.cpp:7881-7882`) cannot be popped — that's what the `> 1` check enforces. Use `ImScoped::ID` and you cannot misbalance: every constructor pushes, every destructor pops, even on early return or exception.

### Debugging tools

**`ImGui::GetID("label")`** returns the hash that *would* be assigned to a widget with that label at the current stack position, without affecting state. Drop a `ImGuiID id = ImGui::GetID("My value"); std::println("{}", id);` next to the suspicious widget, then compare against the hash on the loop iterations that misbehave.

**ID Stack Tool.** Open the metrics window (`ImGui::ShowMetricsWindow()`) and toggle "Show ID Stack Tool", or call `ImGui::ShowIDStackToolWindow()` directly (`imgui.h:417`, `imgui.cpp:24139`). Hover any widget in your app and the tool walks every level of the hash chain that built its ID — window name, each `PushID`, each container's implicit push, the final label. The demo wires it up at `imgui_demo.cpp:786` (`ImGui::MenuItem("ID Stack Tool", ...)`). This is the single most useful tool for diagnosing collisions; reach for it before guessing.

**`io.ConfigDebugHighlightIdConflicts`** (since v1.91, see `imgui.h` IO struct). When enabled, ImGui tints any widget that has a colliding ID with a debug overlay. Combine with the ID Stack Tool to confirm which submissions are colliding.

**DebugLog.** `ImGui::DebugLog(...)` (`imgui.h:1181`) feeds `ShowDebugLogWindow` (`imgui.h:416`); set `IO.ConfigDebugLogEvents` flags to surface ID-related events (active ID changes, focus moves) into the log without writing manual prints.

---

## Tier 3 — Appendix

### The v1.92.6 hash change (upgrade hazard)

ImGui v1.92.6 (released 2026-02-17, see `CHANGELOG.txt:220`) changed how the `###` operator participates in hashing. Per `CHANGELOG.txt:282-289`:

```
Before: GetID("Hello###World") == GetID("###World") != GetID("World")
After:  GetID("Hello###World") == GetID("###World") == GetID("World")
```

The `###` characters themselves are no longer included in the output hash. The changelog explicitly notes: "This will invalidate hashes (stored in .ini data) for Tables and Windows that are using the `###` operators." If you upgrade across this boundary, users' saved `.ini` settings for any window or table whose ID uses `###` will not load — the windows will appear at their default position/size on first launch after upgrade. This is a one-time event per user; flag it in your release notes.

### See also

- [layout-and-sizing.md](layout-and-sizing.md) — some "my widget keeps growing/resetting" surprises trace back to ID changes (e.g. a child window whose `str_id` varies by frame), not to sizing logic.
- [pitfalls-catalog.md](pitfalls-catalog.md) — cross-cutting index of common ImGui mistakes, with pointers back to the relevant deep-dive.
- [styling-fonts-dpi.md](styling-fonts-dpi.md) section 13 — non-ASCII characters in widget labels (the `Button("X-glyph")` glyph-coverage caveat).
