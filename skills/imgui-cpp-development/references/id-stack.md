# ID stack: how IDs are computed, why collisions happen, how to fix them

> **Load this file when:** debugging "all my buttons do the same thing", "my widget state resets when I scroll", or any other ID-collision symptom; or when authoring widgets in a loop / inside a custom container that needs stable identification.

Dear ImGui identifies every interactive widget by hashing its label combined with the parent ID stack. The mismatch between the user's mental model ("two buttons with the same caption do different things") and the hash result ("identical caption inside identical scope produces an identical ID, so the two widgets are literally the same widget") is the root cause of essentially every confusing-state bug — buttons that share state, sliders that fight each other, text inputs whose focus jumps, tree nodes that collapse when they shouldn't. Once you internalize the hashing model, every "weird ImGui state bug" stops being weird.

## How IDs are computed

A widget's ID is a hash of its label, seeded by the *current* value at the top of the per-window ID stack. From `imgui.cpp:9767` (the `[SECTION] ID STACK` block):

```cpp
ImGuiID ImGuiWindow::GetID(const char* str, const char* str_end)
{
    ImGuiID seed = IDStack.back();
    ImGuiID id = ImHashStr(str, str_end ? (str_end - str) : 0, seed);
    // ...
}
```

The seed (`IDStack.back()`) is whatever the parent context last pushed. When a window is created, its base ID stack entry is the hash of the window's name (`imgui.cpp:4671`: `IDStack.push_back(ID)` in the `ImGuiWindow` constructor, where `ID = ImHashStr(name)`). That base entry is also restored at the start of each `Begin()` call (`imgui.cpp:7881-7882`).

So every widget ID is implicitly: `hash(window_name, ...PushID values..., ...implicit container seeds (e.g. TreeNode)..., widget_label)`. The "parent ID stack" is the literal stack that lives on `ImGuiWindow::IDStack` — it includes the window's own ID at the bottom, every active `PushID` you've made, and any implicit pushes from container widgets like `TreeNode`, `BeginChild`, or popups.

Three overloads of `GetID` exist for the three common identifier shapes — string (`imgui.cpp:9773`), pointer (`imgui.cpp:9785`), integer (`imgui.cpp:9797`) — and they all combine via `ImHashStr` / `ImHashData` against the same seed. `PushID` is the user-facing wrapper that calls `GetID` and pushes the result onto `IDStack` (`imgui.cpp:9828-9858`).

## The three PushID overloads

`PushID(const char*)`, `PushID(const void*)`, and `PushID(int)` all produce a stack entry; they differ only in *what* gets hashed. Pick the one that matches the identity model of your data:

- **`PushID(ptr)` for object-identity.** The address of an owned struct, an `&item` reference, or a stable pointer key. Survives reordering of the underlying container — if you sort or shuffle a `std::vector<Item>`, each item still hashes to the same ID because the address inside `&items[i]` is the address of the moved-into element. Use this whenever you have a real object whose lifetime you control.
- **`PushID(int)` for stable indices.** Fast, no allocation, ideal for fixed-size loops. Breaks the moment you reorder, insert, or delete without rebuilding state — index 3 after a removal refers to a different object than index 3 before the removal, so any per-item state (focus, drag, edit-in-progress) jumps to the wrong row.
- **`PushID(string)` for string-based identity.** User-named entries (file paths, named layers). Convenient and stable across reorder, but more expensive than the pointer or int forms — pick this only when the string already has identity meaning to your data model.

## The "##invisible-suffix" and "###override" suffixes

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

## Common collision patterns (with `ImScoped::ID` fixes)

**1. Same label in a loop — the "all my buttons do the same thing" bug.** From `FAQ.md:283-307`, this is the most common ID mistake in real codebases:

```cpp
// Wrong — all three widgets hash to the same ID:
ImGui::Begin("Incorrect!");
ImGui::DragFloat2("My value", &objects[0]->pos.x);
ImGui::DragFloat2("My value", &objects[1]->pos.x);   // collides
ImGui::DragFloat2("My value", &objects[2]->pos.x);   // collides
ImGui::End();
```

Fix with a per-iteration scope guard:

```cpp
if (auto w = ImScoped::Window("Correct")) {
    for (auto* obj : objects) {
        ImScoped::ID id{obj};               // hash by object identity
        ImGui::DragFloat2("My value", &obj->pos.x);
    }   // ~ID() pops on every iteration, even on early break
}
```

**2. Two windows with identical content widgets.** This is *not* a bug — windows scope IDs. `Button("OK")` inside `Begin("WindowA")` hashes to `hash("WindowA", "OK")`; the same `Button("OK")` inside `Begin("WindowB")` hashes to `hash("WindowB", "OK")`. State does not cross windows. If you genuinely want shared state, you have to wire it up yourself; if state seems to cross, you almost certainly have a single window being submitted twice with the same name (look for misbalanced `Begin`/`End`, or two code paths calling `Begin("Editor")` in the same frame — which makes them the same window, not two).

**3. Tree node state and child widget IDs.** `TreeNode` itself pushes onto the ID stack. So a button inside a tree node hashes as `hash(window, treenode_label, button_label)`. If you change the tree node's label (without `###`), the child widget IDs change too. Usually this is harmless because the tree node was collapsed; but if you're animating the tree node's label, child state will reset. Fix by giving the tree node a stable `###` ID, or by pushing your own `ImScoped::ID` *inside* the tree node so children depend on a stable seed regardless of the tree node label.

**4. Programmatic widget generation across docked layouts.** Docked windows are still distinct windows — they keep their own ID stack rooted at `hash(window_name)`. The collision risk is when you generate the same window *name* twice in different dock nodes (say, two "Properties" panels). Either give each a unique name (`"Properties##selectionA"`) or share state explicitly. If a docked window is re-parented at runtime, IDs do not change — they are tied to the window name, not the dock node.

## Begin/End scoping rule for IDs

The ID stack is per-window. `Begin("X")` resets the working stack to `[hash("X")]` for the duration of the block; `End` pops back to whatever the previous window's stack was. `PushID` and `PopID` must balance *within* the same window scope. Calling `PopID` more times than `PushID` triggers an assert (`imgui.cpp:9897-9902`):

```cpp
void ImGui::PopID()
{
    ImGuiWindow* window = GImGui->CurrentWindow;
    IM_ASSERT_USER_ERROR_RET(window->IDStack.Size > 1, "Calling PopID() too many times!");
    window->IDStack.pop_back();
}
```

The base entry (the window's own ID, pushed by `Begin` at `imgui.cpp:7881-7882`) cannot be popped — that's what the `> 1` check enforces. Use `ImScoped::ID` and you cannot misbalance: every constructor pushes, every destructor pops, even on early return or exception.

## Debugging IDs

**`ImGui::GetID("label")`** returns the hash that *would* be assigned to a widget with that label at the current stack position, without affecting state. Drop a `ImGuiID id = ImGui::GetID("My value"); std::println("{}", id);` next to the suspicious widget, then compare against the hash on the loop iterations that misbehave.

**ID Stack Tool.** Open the metrics window (`ImGui::ShowMetricsWindow()`) and toggle "Show ID Stack Tool", or call `ImGui::ShowIDStackToolWindow()` directly (`imgui.h:417`, `imgui.cpp:24139`). Hover any widget in your app and the tool walks every level of the hash chain that built its ID — window name, each `PushID`, each container's implicit push, the final label. The demo wires it up at `imgui_demo.cpp:786` (`ImGui::MenuItem("ID Stack Tool", ...)`). This is the single most useful tool for diagnosing collisions; use it before guessing.

**DebugLog.** `ImGui::DebugLog(...)` (`imgui.h:1181`) feeds `ShowDebugLogWindow` (`imgui.h:416`); set `IO.ConfigDebugLogEvents` flags to surface ID-related events (active ID changes, focus moves) into the log without writing manual prints.

## The v1.92.6 hash change (upgrade hazard)

ImGui v1.92.6 (released 2026-02-17, see `CHANGELOG.txt:220`) changed how the `###` operator participates in hashing. Per `CHANGELOG.txt:282-289`:

```
Before: GetID("Hello###World") == GetID("###World") != GetID("World")
After:  GetID("Hello###World") == GetID("###World") == GetID("World")
```

The `###` characters themselves are no longer included in the output hash. The changelog explicitly notes: "This will invalidate hashes (stored in .ini data) for Tables and Windows that are using the `###` operators." If you upgrade across this boundary, users' saved `.ini` settings for any window or table whose ID uses `###` will not load — the windows will appear at their default position/size on first launch after upgrade. This is a one-time event per user; flag it in your release notes.

## Idiomatic recipe — list with stable per-item identity

```cpp
for (auto& item : items) {
    ImScoped::ID id{&item};   // hash the address; ordering changes don't break state
    if (ImGui::Selectable(item.name.c_str())) {
        on_select(item);
    }
}
```

Two properties to internalize: (1) `&item` is the address of the `vector` element after any reorder, so sorting `items` does not invalidate per-row state; (2) the destructor of `id` runs on every loop iteration, including on early `break` or thrown exception, so the stack stays balanced without any explicit `PopID`. This is the canonical loop pattern — anywhere you find yourself writing a manual `PushID(i)` / `PopID()` pair around a Selectable, Button, or DragFloat in a loop, prefer this.

## See also

- [layout-and-sizing.md](layout-and-sizing.md) — some "my widget keeps growing/resetting" surprises trace back to ID changes (e.g. a child window whose `str_id` varies by frame), not to sizing logic.
- [pitfalls-catalog.md](pitfalls-catalog.md) — cross-cutting index of common ImGui mistakes, with pointers back to the relevant deep-dive.
