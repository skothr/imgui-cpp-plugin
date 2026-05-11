# Widget recipes

> **Load this file when:** building day-to-day UI — drag-and-drop, color pickers, tooltips, selectable lists, comboboxes (with virtualization), text input with `std::string`, plotting helpers, tree views, disabled blocks, keyboard-focus management. For more exotic / fully-custom widgets see [custom-widgets.md](custom-widgets.md).
>
> **This file is a recipe catalog.** Each section below is a self-contained recipe; you almost never need to load the whole file. Use the symptom-to-recipe table below the Quick navigation to find the one section relevant to your task, then load just that range with `Read offset=N limit=M`.

<!-- QUICK_NAV_BEGIN -->
> **Quick navigation** (jump to a section instead of loading the whole file - `Read offset=N limit=M`):
>
> - L  31-50   Find your recipe by symptom
> - L  51-75   1. Drag and drop
> - L  76-101  2. Color pickers
> - L 102-129  3. Tooltips
> - L 130-152  4. Selectable list with stable IDs
> - L 153-174  5. Combo with virtualization
> - L 175-205  6. `InputText` with `std::string`
> - L 206-222  7. Plotting (`PlotLines`, `PlotHistogram`)
> - L 223-243  8. Tree view
> - L 244-257  9. Disabled blocks
> - L 258-280  10. Keyboard focus
> - L 281-293  11. `InputTextMultiline`
> - L 294-307  12. Progress bar
> - L 308-315  13. File dialogs — explicit non-recipe
> - L 316-346  14. End-to-end: drag-drop reordering of a list
> - L 347-351  See also
<!-- QUICK_NAV_END -->





## Find your recipe by symptom

| User says or implies… | Recipe |
|---|---|
| "drag and drop", "drag to reorder", "drop X on Y" | §1 drag-drop basics; §14 end-to-end reorder |
| "color picker", "swatch button", "hex / HSV input" | §2 |
| "tooltip", "hover hint", "show details on hover" | §3 |
| "list with one selected row", "highlight current item", "scrolling selectable list" | §4 |
| "combo with thousands of entries", "virtualized dropdown" | §5 |
| "InputText with std::string", "resizable text field" | §6 |
| "sparkline", "frame-time graph", "small inline histogram" | §7 |
| "tree view", "collapsible nested list", "TreeNode state resetting" | §8 |
| "disable a widget", "grey out a button", "non-interactive while loading" | §9 |
| "auto-focus search box on open", "set focus to next field", "keyboard focus" | §10 |
| "multi-line text input", "scratch-pad / code-editor field" | §11 |
| "progress bar", "loading bar", "indeterminate spinner" | §12 |
| "file dialog", "open / save picker" | §13 (intentional non-recipe) |

These are the recipes that come up every week. Each one is a stock widget: short, idiomatic, RAII-guarded, with the gotcha that bites people most often called out. When in doubt, the demo (`imgui_demo.cpp`) is the authoritative reference — line ranges in each recipe point at the section that originally inspired it.

## 1. Drag and drop

Drag and drop is a producer/consumer protocol: a "source" widget marks itself draggable, sets a typed payload while the user drags, and a "target" widget accepts the payload on drop. Lifecycle: `BeginDragDropSource → SetDragDropPayload → EndDragDropSource` on the source side; `BeginDragDropTarget → AcceptDragDropPayload → EndDragDropTarget` on the target side. The payload type is a string up to 32 chars; names starting with `_` are reserved by ImGui.

```cpp
ImGui::Button(names[n], ImVec2(60, 60));

if (auto src = ImScoped::DragDropSource()) {
    ImGui::SetDragDropPayload("DND_DEMO_CELL", &n, sizeof(int));
    ImGui::Text("Move %s", names[n]);   // tooltip preview shown while dragging
}

if (auto dst = ImScoped::DragDropTarget()) {
    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("DND_DEMO_CELL")) {
        IM_ASSERT(p->DataSize == sizeof(int));
        int from = *(const int*)p->Data;
        std::swap(names[from], names[n]);
    }
}
```

Useful flags on `BeginDragDropSource`: `ImGuiDragDropFlags_SourceAllowNullID` lets you drag from items that don't have an ID (like `Text` calls), `_SourceNoPreviewTooltip` suppresses the auto-tooltip if you'd rather draw your own, and `_SourceNoHoldToOpenOthers` prevents the drag-hold from auto-opening tree nodes/tabs as you hover. On `AcceptDragDropPayload`: `_AcceptBeforeDelivery` lets you peek at the payload to render a "yes I'd accept this" highlight before the user releases the mouse.

Gotcha: payload data is **copied and held by ImGui** until drop, so passing pointers to short-lived locals is safe — but if you need to drag a large object, pass an index/handle, not the object itself. The payload type string is also limited to 32 chars including the null terminator. See `imgui_demo.cpp:1639-1705` for the full copy/move/swap demo, and `4267-4310` for tree-node drag sources.

## 2. Color pickers

`ColorEdit3/4` is the compact one-line editor with a popup picker; `ColorPicker3/4` is the full picker (hue bar/wheel, sat/val square, RGB/HSV inputs). Both auto-register as drag-drop sources/targets using the reserved payload types `IMGUI_PAYLOAD_TYPE_COLOR_3F` and `_COLOR_4F`, so dragging a color from one widget to another works for free. Common flags: `ImGuiColorEditFlags_NoAlpha`, `_HDR`, `_DisplayHex`, `_PickerHueWheel`.

```cpp
static float col[4] = {0.4f, 0.7f, 0.0f, 0.5f};
ImGui::ColorEdit4("tint", col,
                  ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_AlphaBar);

ImGui::ColorPicker3("hue", col,
                    ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview);
```

If you only need the picker as a popup attached to some other UI element (e.g., a swatch button), the trick is `ColorButton` followed by an `OpenPopup`/`BeginPopup` pair containing `ColorPicker3/4`:

```cpp
if (ImGui::ColorButton("##swatch", ImVec4(col[0], col[1], col[2], col[3]))) {
    ImGui::OpenPopup("color_picker");
}
if (auto pop = ImScoped::Popup("color_picker")) {
    ImGui::ColorPicker4("##picker", col, ImGuiColorEditFlags_NoSidePreview);
}
```

Gotcha: HDR colors (values > 1.0) are clamped silently unless `ImGuiColorEditFlags_HDR` is set. The widgets also gamma-correct on display by default; if your project stores linear-light colors, pass them straight through and the picker will linearize/de-linearize for the user. See `imgui_demo.cpp:1173-1360` for the full options tour.

## 3. Tooltips

Three layers of API, used for different needs:

- `SetItemTooltip("...")` / `SetTooltip("...")` — fire-and-forget single text tooltip. `SetItemTooltip` only fires when the previous item is hovered.
- `BeginItemTooltip()` — `if (IsItemHovered(ImGuiHoveredFlags_ForTooltip) && BeginTooltip())` shorthand. Use when you need multiple widgets inside the tooltip.
- `BeginTooltip()` — manual; you control the hover check and lifecycle yourself.

```cpp
ImGui::Button("Save");
ImGui::SetItemTooltip("Saves the current document (Ctrl+S)");

ImGui::Button("Stats");
if (auto tt = ImScoped::ItemTooltip()) {     // only enters when hovered
    ImGui::Text("Frames: %d", frame_count);
    ImGui::Separator();
    ImGui::PlotLines("##fps", fps_history, IM_ARRAYSIZE(fps_history));
}

if (ImGui::IsItemHovered() && some_condition) {
    if (auto tt = ImScoped::Tooltip()) {     // unconditional Begin/End pair
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Conditional warning");
    }
}
```

Gotcha: `SetTooltip` overrides any previously submitted tooltip text — see comment at `imgui.h:835`. If two `SetTooltip` calls run in the same frame, only the last wins.

## 4. Selectable list with stable IDs

The most common list pattern: a scrolling region of `Selectable` rows where exactly one is "current." Push a stable ID per row (so reordering doesn't break focus), call `SetItemDefaultFocus()` on the active row so keyboard nav lands there when the surrounding popup or combo opens, and pass `ImGuiSelectableFlags_AllowDoubleClick` if you want both single- and double-click semantics.

```cpp
if (auto child = ImScoped::Child("##list", ImVec2(0, -1), ImGuiChildFlags_Borders)) {
    for (size_t i = 0; i < items.size(); ++i) {
        ImScoped::ID row_id{static_cast<int>(items[i].stable_id)};
        const bool is_selected = (selected_id == items[i].stable_id);
        if (ImGui::Selectable(items[i].label.c_str(), is_selected,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            selected_id = items[i].stable_id;
            if (ImGui::IsMouseDoubleClicked(0)) open(items[i]);
        }
        if (is_selected) ImGui::SetItemDefaultFocus();
    }
}
```

For multi-select with shift-range and ctrl-toggle semantics, the modern API is `BeginMultiSelect` / `EndMultiSelect` (added in v1.89.7+); the multi-select demo at `imgui_demo.cpp:3437-3460+` shows the pattern, including the drag-drop interplay where dragging a selected item drags all selected items as one payload. For ad-hoc cases where you don't need range select, manually toggling on `ImGui::GetIO().KeyCtrl` against your own selection set is fine.

Gotcha: when items can be added/removed mid-frame, the ID has to be derived from a stable identity (an `id` field, a pointer, a UUID), not the loop index. If you push the loop index instead, deleting item N causes item N+1 to "inherit" item N's selection/focus state on the next frame, which looks like a bug in your selection code but is actually an ID-stack issue. See `imgui_demo.cpp:1410-1415` and the surrounding combo example for the canonical idiom.

## 5. Combo with virtualization

For a thousand+ items, `BeginCombo` + manual `Selectable` submission scales fine if you let `ImGuiListClipper` skip the off-screen rows. The clipper does the layout-only pass to compute item count and scroll position, then asks you to submit only `[DisplayStart, DisplayEnd)`. Submitting widgets for every item still works but spends layout time linearly with item count, which gets noticeable past a few thousand.

```cpp
if (auto cb = ImScoped::Combo("##huge", current_label)) {
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(items.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            ImScoped::ID row{i};
            const bool is_selected = (selected_idx == i);
            if (ImGui::Selectable(items[i].c_str(), is_selected))
                selected_idx = i;
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
    }
}
```

Gotcha: if you need to ensure a specific item (e.g., the currently-selected one) isn't skipped — important if you read `IsItemHovered` against it later — call `clipper.IncludeItemByIndex(idx)` before `Step()`. See the clipper section near `imgui_demo.cpp:1397-1450`.

## 6. `InputText` with `std::string`

ImGui's `InputText` takes a raw `char*` buffer for portability and to avoid forcing a particular string class on users. The shipped wrapper at `misc/cpp/imgui_stdlib.h` adds `std::string` overloads that handle the resize callback for you. From `docs/FAQ.md:670`:

> To use ImGui::InputText() with a std::string or any resizable string class, see [misc/cpp/imgui_stdlib.h](https://github.com/ocornut/imgui/blob/master/misc/cpp/imgui_stdlib.h).

```cpp
#include "misc/cpp/imgui_stdlib.h"   // pulls in the std::string overloads

std::string name = "Hello";
ImGui::InputText("name", &name);
ImGui::InputTextMultiline("body", &body, ImVec2(-1, 200));
```

If `imgui_stdlib.h` isn't an option (custom string type, restricted vendor tree), wire `ImGuiInputTextFlags_CallbackResize` yourself:

```cpp
auto resize_cb = [](ImGuiInputTextCallbackData* d) -> int {
    if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* s = static_cast<std::string*>(d->UserData);
        s->resize(d->BufTextLen);
        d->Buf = s->data();
    }
    return 0;
};
ImGui::InputText("name", name.data(), name.capacity() + 1,
                 ImGuiInputTextFlags_CallbackResize, resize_cb, &name);
```

Gotcha: the FAQ also warns that heap-allocating `std::string` per-frame in a UI-heavy app can hurt perf. For hot lists, fixed-size `char[256]` or the [Str](https://github.com/ocornut/Str) helper is what `imgui` itself recommends.

## 7. Plotting (`PlotLines`, `PlotHistogram`)

These two are tiny inline visualizers — frame-time graphs, signal-meter histograms, anything where you want a sparkline next to other widgets. Both take a strided float array, an optional overlay text, scale min/max, and a graph size. They are intentionally minimal; for axis labels, legends, zoom, multiple series, etc., reach for the sibling library [ImPlot](https://github.com/epezent/implot), which is the de facto extension for real plotting.

```cpp
ImGui::PlotLines("Frame ms", frame_ms, IM_ARRAYSIZE(frame_ms),
                 0,            // values_offset (rolling buffer support)
                 "fps",        // overlay text
                 0.0f, 33.3f,  // y-range
                 ImVec2(0, 80));

ImGui::PlotHistogram("Bins", bins, IM_ARRAYSIZE(bins), 0, nullptr,
                     0.0f, 1.0f, ImVec2(0, 80));
```

Gotcha: when `scale_min == scale_max == FLT_MAX`, ImGui computes the range from the data — handy for unknown signals, but it makes the y-axis wobble frame-to-frame. Pin it explicitly when stability matters. See `imgui_demo.cpp:2060-2125`.

## 8. Tree view

`TreeNode` opens a collapsible section and returns `true` while the user has it expanded. **Only call `TreePop` when `TreeNode` returned `true`** — it's not idempotent. If your label is dynamic (changes between frames, e.g., contains a counter), the auto-derived ID changes too and the tree's open/closed state resets. Fix by either pushing a stable ID with `ImScoped::ID` and using `"##label"` to suppress the visible name from the ID hash, or using `"###override"` which keeps the ID stable while letting the visible label change.

```cpp
for (auto& node : tree.nodes) {
    ImScoped::ID id{node.uid};                     // stable across renames
    if (ImGui::TreeNode("##node", "%s (%zu kids)", node.name.c_str(), node.children.size())) {
        for (auto& child : node.children)
            ImGui::BulletText("%s", child.name.c_str());
        ImGui::TreePop();                          // only when TreeNode == true
    }
}
```

For very large trees (thousands of nodes), pair the iteration with `ImGuiListClipper` exactly like the combo recipe — but compute a flat list of currently-visible nodes (i.e., honor expand/collapse state) before clipping, since the clipper's index-based virtualization assumes a linear array. The demo at `imgui_demo.cpp:4208-4220+` shows the flattening pattern for a virtualized tree.

`TreeNodeEx(label, flags)` exposes the underlying knobs: `ImGuiTreeNodeFlags_DefaultOpen`, `_OpenOnArrow` (only the arrow toggles, click on the label fires as a normal selectable), `_OpenOnDoubleClick`, `_SpanFullWidth` (highlight extends across the row), `_Leaf` (no expand arrow, useful for leaf rows that should still look like tree items), and `_NoTreePushOnOpen` (don't auto-indent the children — handy when you're rendering rows in a table).

Gotcha: an early `return` between `TreeNode` returning true and `TreePop` will assert on the next frame — wrap the body so every path reaches `TreePop`. There is no `ImScoped::TreeNode` because the conditional behavior on returning true makes it awkward; consider adding one in your project if your team trips over it. See `imgui_demo.cpp:1125-1170` and the larger tree demo around `4149+`.

## 9. Disabled blocks

`BeginDisabled(true)` makes every widget submitted before the matching `EndDisabled` non-interactive and visually dimmed (multiplied by `style.DisabledAlpha`). `BeginDisabled(false)` is a no-op so you can always wrap conditional UI in the guard without branching. Tooltips are exempt — they still appear on hover so users can read why the widget is disabled. Easy to forget `EndDisabled` after an early return; the RAII guard removes the foot-gun:

```cpp
{
    ImScoped::Disabled d{!user.is_admin};
    if (ImGui::Button("Delete account")) confirm_delete();
    ImGui::Checkbox("Hard delete", &hard_delete);
}
```

Gotcha: `IsItemHovered()` returns `false` on disabled items by default. Pass `ImGuiHoveredFlags_AllowWhenDisabled` if you want a tooltip on a disabled control explaining *why* it's disabled. See `imgui_demo.cpp:1606-1614`.

## 10. Keyboard focus

`SetKeyboardFocusHere(offset = 0)` queues focus to be granted to the next-submitted widget (offset 0), or N widgets ahead, or to the previous widget (offset -1). It has to be called **before** the widget you want to focus, in the same frame. `ImGui::ActivateItemByID(id)` (internal API) presses an item programmatically — useful for menu shortcuts that should behave as if the user clicked the item.

```cpp
if (just_opened_search) ImGui::SetKeyboardFocusHere();
ImGui::InputText("##search", &search_text);

if (ImGui::Shortcut(ImGuiKey_F2)) ImGui::ActivateItemByID(rename_field_id);
```

A common pattern: when a search modal opens, focus the search input on the first frame so the user can start typing immediately. Track a "just opened" flag in your model:

```cpp
if (auto m = ImScoped::PopupModal("Quick Open")) {
    if (just_opened) { ImGui::SetKeyboardFocusHere(); just_opened = false; }
    ImGui::InputText("##q", &query);
    // ... results below ...
}
```

Gotcha: focus requests are queued and processed on the next frame. Calling `SetKeyboardFocusHere()` and then immediately reading `IsItemFocused()` will read the previous frame's state. If you need to know whether a focus request you just queued was *delivered*, check `IsItemFocused()` on the next frame, not in the same submission.

## 11. `InputTextMultiline`

The multi-line variant takes the same buffer + flags as `InputText`, plus a size. The size argument is doing more work than it looks like: pass `ImVec2(0, 0)` and the widget auto-grows with content, which inside a child or scroll region usually expands forever. For predictable layout, pass an explicit size.

```cpp
std::string body;
ImGui::InputTextMultiline("##body", &body,
                          ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8),
                          ImGuiInputTextFlags_AllowTabInput);
```

Gotcha: the auto-grow trap is one of the recurring "my window keeps getting taller every frame" bugs — see [layout-and-sizing.md](layout-and-sizing.md) for the broader pattern.

## 12. Progress bar

`ProgressBar(fraction, size_arg = ImVec2(-1,0), overlay = nullptr)`. `fraction < 0` produces an indeterminate animated stripe. The overlay text is centered on the bar; pass `nullptr` for the default percentage display.

```cpp
ImGui::ProgressBar(downloaded / (float)total,
                   ImVec2(-FLT_MIN, 0),
                   std::format("{:.1f} / {:.1f} MB", downloaded / 1e6, total / 1e6).c_str());

ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(0, 0), "Searching…");
```

Gotcha: the indeterminate stripe is driven off `GetTime()`, so it freezes when ImGui's frame doesn't tick.

## 13. File dialogs — explicit non-recipe

Dear ImGui ships **no** file dialog. The three real options:

1. **Roll your own** with `Selectable` rows in a child window. Doable in 50–100 lines for read-only browsing; a write-side dialog with rename/overwrite is closer to a small project. The demo's selectable list at `imgui_demo.cpp:2386-2540+` is a reasonable starting point.
2. **Use a community library.** [imgui-filebrowser](https://github.com/AirGuanZ/imgui-filebrowser) is header-only and the most-cited choice. It's pure ImGui (no native), so it works inside multi-viewport setups and looks consistent with the rest of your UI.
3. **Call the native OS dialog** — Win32 `GetOpenFileName`, GTK/`zenity` on Linux, NSOpenPanel on macOS, or the cross-platform [nfd](https://github.com/btzy/nativefiledialog-extended) wrapper. Recommended for production tools: users expect their OS's file dialog (recent files, sidebar, network mounts, sandbox prompts), and re-implementing that surface in ImGui is a long road for low payoff.

## 14. End-to-end: drag-drop reordering of a list

A complete idiomatic recipe — RAII guards on every Begin/Push, drag-drop using ImGui's payload protocol, applies the reorder once on accept. Drop this into a Begin/End window block:

```cpp
for (size_t i = 0; i < items.size(); ++i) {
    ImScoped::ID row_id{static_cast<int>(items[i].stable_id)};
    ImGui::Selectable(items[i].label.c_str());

    if (auto src = ImScoped::DragDropSource()) {
        ImGui::SetDragDropPayload("ROW_REORDER", &i, sizeof(size_t));
        ImGui::Text("Move %s", items[i].label.c_str());
    }
    if (auto dst = ImScoped::DragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ROW_REORDER")) {
            IM_ASSERT(p->DataSize == sizeof(size_t));
            const size_t from = *(const size_t*)p->Data;
            if (from != i) {
                auto moved = items[from];
                items.erase(items.begin() + from);
                items.insert(items.begin() + (from < i ? i - 1 : i), std::move(moved));
            }
        }
    }
}
```

Why this shape: the source uses the row's index (a stable handle for a single frame) as the payload, which is enough — the target reads it back, computes the corrected destination index (accounting for the gap left when removing the source), and does the move once. Wrapping the source/target in `ImScoped::DragDropSource` / `ImScoped::DragDropTarget` makes it impossible to forget `EndDragDropSource`/`EndDragDropTarget` on an early-return path inside the body. The `ImScoped::ID` keeps focus and selection stable across the reorder by tying the row's ID to its stable identity, not its index.

Gotcha: this fires the swap immediately on drop. If you want a confirm step, store the pending move in your model and trigger it from a modal — see [modals-and-popups.md](modals-and-popups.md). Also note that during the single frame when a row is being dragged, the same row is briefly submitted twice (once at its old position, once as the drag tooltip), which can trigger an ID conflict assert; the demo's "Drag to reorder" example at `imgui_demo.cpp:1707+` works around this with `ImGui::PushItemFlag(ImGuiItemFlags_AllowDuplicateId, true)` for exactly this reason.

## See also

- [custom-widgets.md](custom-widgets.md) — when none of these recipes fit and you need to author from `ItemSize`/`ItemAdd` upward.
- [tables.md](tables.md) — `BeginTable` for grids; selectable rows in tables have specific flags (`ImGuiSelectableFlags_SpanAllColumns`).
- [id-stack.md](id-stack.md) — why `PushID(stable_id)` matters for drag-drop reordering and selectable lists.
