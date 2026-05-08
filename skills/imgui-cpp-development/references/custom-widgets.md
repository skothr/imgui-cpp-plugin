# Custom widgets, DrawList, hit-testing

> **Load this file when:** authoring a non-standard widget — anything that needs the internal item-protocol (item registration, hit-testing, focus, keyboard nav) or low-level DrawList primitives (custom rendering inside or outside any window).
>
> **Shape:** TLDR with the canonical 6-step item-protocol skeleton at the top, followed by per-API surfaces (`IsItem*` queries, keyboard nav, DrawList lists, primitives, paths, clipping, channels, textures), then pitfalls. Most authoring questions need only the TLDR plus one API surface.

<!-- QUICK_NAV_BEGIN -->
> **Quick navigation** (jump to a section instead of loading the whole file - `Read offset=N limit=M`):
>
> - L  33-65   TLDR — minimal correct custom widget
> - L  66-102  The item protocol — what every custom widget must do
> - L 103-151  Walk-through: `ImGui::ButtonEx` (imgui_widgets.cpp:782-821)
> - L 152-169  The `IsItem*` query family
> - L 170-175  Keyboard nav opt-in
> - L 176-185  DrawList — three lists, three layers
> - L 186-209  DrawList primitives (terse table)
> - L 210-227  The Path API (imgui.h:3495-3506)
> - L 228-240  `PushClipRect` / `PopClipRect` — two functions, two semantics
> - L 241-256  `ChannelsSplit` / `ChannelsMerge` — out-of-order rendering
> - L 257-260  Custom-rendering demo
> - L 261-272  `ImTextureID` type mismatches
> - L 273-284  Common pitfalls (each with reproducer + fix)
> - L 285-289  See also
<!-- QUICK_NAV_END -->







Dear ImGui's stock widgets all sit on top of three internal-API calls (`ItemSize`, `ItemAdd`, `ButtonBehavior`) plus an `ImDrawList`. If you skip any of them, the parts you skipped silently break: a missing `ItemSize` means the next widget overlaps yours, a missing `ItemAdd` means hit-testing and keyboard nav don't see your item at all, and rendering before `ItemAdd` bypasses clipping. Read this file before adding the seventh helper that "almost works."

## TLDR — minimal correct custom widget

Every conformant widget follows this 6-step skeleton (the worked walk-through of `ButtonEx` below shows the same pattern with rendering filled in):

```cpp
bool MyWidget(const char* label, ImVec2 size_arg /* ... */) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;                              // 1. Window collapsed -> bail.

    const ImGuiID id = window->GetID(label);                          // 2. Hash label vs ID stack.
    const ImVec2 pos  = window->DC.CursorPos;
    const ImVec2 size = ImGui::CalcItemSize(size_arg, /* default w/h */ 100.0f, 24.0f);
    const ImRect bb(pos, pos + size);

    ImGui::ItemSize(size);                                            // 3. Reserve layout space.
    if (!ImGui::ItemAdd(bb, id)) return false;                        // 4. Register; bail if clipped.

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);    // 5. Drive interaction.

    ImDrawList* dl = ImGui::GetWindowDrawList();                      // 6. Render last.
    dl->AddRectFilled(bb.Min, bb.Max,
                      ImGui::GetColorU32(held ? ImGuiCol_ButtonActive
                                       : hovered ? ImGuiCol_ButtonHovered
                                                 : ImGuiCol_Button));
    return pressed;
}
```

Why each step: `SkipItems` skips submission inside collapsed/clipped windows; `GetID` hashes the label so two of these in the same loop don't collide (or wrap the call site in `ImScoped::ID`); `ItemSize` advances the layout cursor for *every* widget including clipped ones, so the cursor stays consistent; `ItemAdd` is the gate that registers with hit-testing, keyboard nav, and clipping — when it returns false you must skip rendering and state queries; `ButtonBehavior` consumes mouse state and writes back to `g.LastItemData`; rendering goes last so it can read the freshly-computed interaction state. Custom widgets that follow this skeleton are indistinguishable from stock widgets to the rest of ImGui.

When the widget needs `IsItemHovered`/`IsItemActive`-style queries, additional API surfaces (`IsItem*` family, keyboard-nav opt-in, DrawList layers, path API, clipping, channels), or specific drawing primitives, jump to the relevant section below.

## The item protocol — what every custom widget must do

The standard sequence (see `ButtonEx` at `imgui_widgets.cpp:782-821` for the canonical example):

1. `ImScoped::ID id{...}` if you're inside a loop or otherwise need to disambiguate.
2. Compute `ImRect bb = { pos, pos + size };` in absolute screen space.
3. `ImGui::ItemSize(size, baseline_y)` — advance the layout cursor and reserve space.
4. `if (!ImGui::ItemAdd(bb, id)) return;` — register with the framework; bail when clipped.
5. `ImGui::ButtonBehavior(bb, id, &hovered, &held, flags)` — drive press/release/click/hold state.
6. Submit drawing via `ImGui::GetWindowDrawList()`.

The signatures (from `imgui_internal.h:3534-3537`):

```cpp
IMGUI_API void  ItemSize(const ImVec2& size, float text_baseline_y = -1.0f);
inline   void  ItemSize(const ImRect& bb, float text_baseline_y = -1.0f);
IMGUI_API bool ItemAdd(const ImRect& bb, ImGuiID id, const ImRect* nav_bb = NULL,
                       ImGuiItemFlags extra_flags = 0);
IMGUI_API bool ItemHoverable(const ImRect& bb, ImGuiID id, ImGuiItemFlags item_flags);
```

`ButtonBehavior` lives at `imgui_internal.h:3953`:

```cpp
IMGUI_API bool ButtonBehavior(const ImRect& bb, ImGuiID id,
                              bool* out_hovered, bool* out_held,
                              ImGuiButtonFlags flags = 0);
```

The overload comment on the `ImRect` form of `ItemSize` at `imgui_internal.h:3535` is worth quoting verbatim:

> // FIXME: This is a misleading API since we expect CursorPos to be bb.Min.

In practice that means: pass the **size**, not the bounding box, unless you've already moved `DC.CursorPos` to `bb.Min`. The `ImVec2` form is the safe default.

`ItemAdd` does the clipping — when it returns `false`, the item is outside the visible area (or otherwise excluded), and you must skip rendering and state queries. It also stores per-item state in `g.LastItemData`, which the entire `IsItem*` query family reads from.

## Walk-through: `ImGui::ButtonEx` (imgui_widgets.cpp:782-821)

```cpp
bool ImGui::ButtonEx(const char* label, const ImVec2& size_arg, ImGuiButtonFlags flags) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;     // 1. Window is collapsed/clipped — bail.

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label); // 2. Hash the label against the ID stack.
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos  = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg,
                               label_size.x + style.FramePadding.x * 2.0f,
                               label_size.y + style.FramePadding.y * 2.0f);
    const ImRect bb(pos, pos + size);

    ItemSize(size, style.FramePadding.y);    // 3. Reserve layout space first.
    if (!ItemAdd(bb, id)) return false;      // 4. Register; bail if clipped.

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);  // 5. Interaction.

    const ImU32 col = GetColorU32(
        (held && hovered) ? ImGuiCol_ButtonActive
                          : hovered ? ImGuiCol_ButtonHovered
                                    : ImGuiCol_Button);
    RenderNavCursor(bb, id);                 // 6. Render last — after state is known.
    RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding,
                      label, NULL, &label_size, style.ButtonTextAlign, &bb);
    return pressed;
}
```

Why this exact order: layout (`ItemSize`) has to run for every widget, even clipped ones, so the cursor advances consistently. `ItemAdd` is the gate — it both registers the item and reports clipping. `ButtonBehavior` consumes mouse state and writes back to `g.LastItemData`. Rendering reads the freshly-computed interaction state, so it has to come last. Custom widgets that follow this skeleton are indistinguishable from stock widgets to the rest of ImGui.

For loops over many items, wrap each iteration in an `ImScoped::ID`:

```cpp
for (int i = 0; i < items.size(); ++i) {
    ImScoped::ID id{i};
    MyCustomWidget(items[i]);
}
```

Without the push, every iteration hashes to the same ID and ImGui treats them as one item — interaction breaks completely.

## The `IsItem*` query family

All of these read `g.LastItemData`, populated by the most recent `ItemAdd` call. From `imgui.h:1045-1060`:

- `IsItemHovered(flags = 0)` — last item is hovered AND not blocked by a popup, disabled state, etc.
- `IsItemActive()` — currently being interacted with (held button, edited text field).
- `IsItemClicked(button = 0)` — hovered and clicked this frame. Note the function comment: not equivalent to `Button()`'s return value.
- `IsItemFocused()` — has keyboard/gamepad nav focus.
- `IsItemDeactivated()` — was active last frame, isn't this frame. Useful for Undo/Redo commit points.
- `IsItemDeactivatedAfterEdit()` — same plus value changed. May fire false positives on `Combo`/`ListBox`/`Selectable` when you click an already-selected entry.
- `GetItemRectMin()` / `GetItemRectMax()` / `GetItemRectSize()` — screen-space bounds of the last item.

Because they all target **the last submitted item**, calling any of them after submitting an unrelated widget gives you state from that unrelated widget. Group your queries immediately after the relevant submission, or wrap a logical unit in `ImScoped::Group` and query the group instead.

### Hit-testing nuances

`IsItemHovered` automatically respects ImGui's overlay/popup occlusion: if a popup is in front of your widget, it returns `false` even when the mouse is over the underlying rectangle. `ImGui::IsMouseHoveringRect(bb.Min, bb.Max)` does **not** — it's pure geometry. Prefer the framework function unless you genuinely need to test against a rectangle that isn't an item, in which case combine geometry with `IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)` or similar.

## Keyboard nav opt-in

For keyboard nav to land on your widget, `ItemAdd` has to register it with the nav system, which it does automatically when the window has nav enabled and the item isn't flagged `ImGuiItemFlags_NoNav`. Modern v1.92 nav uses the `ImGuiNavMoveFlags_*` enum (declared at `imgui_internal.h:1747-1764`) — `LoopX`/`WrapY`/`Activate`/`IsTabbing` and so on. The legacy `ImGuiNavInput` API is deprecated; don't author against it.

If your widget bundles multiple sub-items (a row that contains a button plus a slider, say), call `ImGui::SetNavFocusScope()` to give them their own focus group, and `ImGui::SetItemDefaultFocus()` on the entry that should receive focus when the scope is first reached.

## DrawList — three lists, three layers

All three return `ImDrawList*` and use **absolute screen-space** coordinates. Use `GetCursorScreenPos()` to convert local positions.

- `ImGui::GetWindowDrawList()` (`imgui.h:473`) — the current window's draw list, clipped to the window's body. Your default for custom widget rendering.
- `ImGui::GetForegroundDrawList(viewport = nullptr)` (`imgui.h:1072`) — drawn after every window. HUD overlays, debug crosshairs, anything that should always be on top.
- `ImGui::GetBackgroundDrawList(viewport = nullptr)` (`imgui.h:1071`) — drawn before every window. Wallpaper, world-space gizmos, grid lines.

Don't call `GetWindowDrawList()` after `ImGui::End()` — at that point the "current window" is whatever runs next, and you'll write into the wrong list (or none, depending on timing). Either grab the pointer inside the `Begin/End` block, or use the foreground/background lists when you need to draw outside.

## DrawList primitives (terse table)

Signatures in `imgui.h:3458-3490`. Coordinates are screen-space; `col` is `ImU32` (`IM_COL32(r, g, b, a)`).

| Call | What it draws |
|---|---|
| `AddLine(p1, p2, col, thickness)` | Single line segment. |
| `AddRect(p_min, p_max, col, rounding, flags, thickness)` | Outline rectangle, optionally rounded. |
| `AddRectFilled(p_min, p_max, col, rounding, flags)` | Filled rectangle. |
| `AddRectFilledMultiColor(p_min, p_max, c_tl, c_tr, c_br, c_bl)` | Per-corner gradient fill. |
| `AddCircle(center, r, col, num_segments=0, thickness)` | Outline circle (`num_segments=0` auto-tessellates). |
| `AddCircleFilled(center, r, col, num_segments=0)` | Filled circle. |
| `AddNgon(...)` / `AddNgonFilled(...)` | Regular N-gon. |
| `AddEllipse(...)` / `AddEllipseFilled(...)` | Ellipse with rotation. |
| `AddTriangle(p1, p2, p3, col, thickness)` / `AddTriangleFilled(...)` | Triangle. |
| `AddPolyline(points, n, col, flags, thickness)` | Open or closed polyline (use `ImDrawFlags_Closed`). |
| `AddConvexPolyFilled(points, n, col)` | Filled convex polygon (fast). |
| `AddConcavePolyFilled(points, n, col)` | Filled concave polygon (slower, O(N²)). |
| `AddBezierCubic(p1, p2, p3, p4, col, thickness, num_segments=0)` | 4-point cubic Bezier. |
| `AddBezierQuadratic(p1, p2, p3, col, thickness, num_segments=0)` | 3-point quadratic Bezier. |
| `AddImage(tex, p_min, p_max, uv_min, uv_max, col)` | Textured quad. |
| `AddImageQuad(...)` / `AddImageRounded(...)` | Quad with per-corner UVs / rounded textured quad. |
| `AddText(pos, col, text)` | Text at a screen-space position. |

## The Path API (imgui.h:3495-3506)

The path API accumulates points into the draw list's internal `_Path` buffer, then closes them with a single fill or stroke call. Use it when you need a single closed shape composed of several curve segments — a rounded notch, a tab outline, a custom marker.

```cpp
ImDrawList* dl = ImGui::GetWindowDrawList();
dl->PathLineTo(a);
dl->PathArcTo(center, radius, 0.0f, IM_PI);
dl->PathBezierCubicCurveTo(c1, c2, b);
dl->PathLineTo(a);
dl->PathStroke(IM_COL32(255, 200, 0, 255), ImDrawFlags_Closed, 2.0f);
// or: dl->PathFillConvex(IM_COL32(255, 200, 0, 64));
```

`PathStroke`/`PathFillConvex` both reset the path buffer afterwards, so you can chain shapes back-to-back. Use `PathArcToFast(center, r, a_min_of_12, a_max_of_12)` for circles and rounded corners — it skips the trig and uses a precomputed 12-step table.

For one-off shapes, the `Add*` primitives are simpler. Reach for the path API only when you genuinely need multi-segment composition.

## `PushClipRect` / `PopClipRect` — two functions, two semantics

There are two `PushClipRect` calls, and the difference matters:

- `ImGui::PushClipRect(min, max, intersect)` (`imgui.h:1029`) — the **logic-level** clip. Affects hit-testing AND rendering. Items submitted while it's active that fall entirely outside the clip rect are culled by `ItemAdd`.
- `ImDrawList::PushClipRect(min, max, intersect=false)` (`imgui.h:3446`) — the **render-level** clip only. Pixels outside the rect aren't drawn, but `IsItemHovered` etc. don't know about it.

The `imgui.h:3446` comment puts it bluntly:

> Render-level scissoring. This is passed down to your render function but not used for CPU-side coarse clipping. Prefer using higher-level ImGui::PushClipRect() to affect logic (hit-testing and widget culling).

If you push a render-only clip and then submit an item, the item will *look* clipped but still be hit-testable in the clipped region — which is almost never what you want. Use the framework call (`ImGui::PushClipRect`, or `ImScoped::ClipRect`) for anything that interacts.

## `ChannelsSplit` / `ChannelsMerge` — out-of-order rendering

`ImDrawList::ChannelsSplit(n)`, `ChannelsSetCurrent(i)`, and `ChannelsMerge()` (`imgui.h:3532-3534`) let you submit draw commands into N parallel channels and then flatten them back into one ordered list. Lower-numbered channels render first (i.e., behind). Useful when you want to draw a background after the content that determines its size (the demo shows this at `imgui_demo.cpp:10449-10460`):

```cpp
ImDrawList* dl = ImGui::GetWindowDrawList();
dl->ChannelsSplit(2);
dl->ChannelsSetCurrent(1);                                    // foreground content
dl->AddRectFilled(p, p + ImVec2(50, 50), IM_COL32(0, 0, 255, 255));
dl->ChannelsSetCurrent(0);                                    // background drawn later, displayed first
dl->AddRectFilled(p + ImVec2(25, 25), p + ImVec2(75, 75), IM_COL32(255, 0, 0, 255));
dl->ChannelsMerge();                                          // flatten back
```

This is what the Tables API uses internally — one channel per column, merged at end of frame.

## Custom-rendering demo

The single most useful reference is `ShowExampleAppCustomRendering` at `imgui_demo.cpp:10167-10464+`. It demonstrates every primitive, gradient, anti-aliasing flag, clipping case, image draw, and channel split. Read it once before authoring anything non-trivial; it will save you a half-day of stack-overflow archaeology.

## `ImTextureID` type mismatches

`ImTextureID` is a backend-defined opaque handle. The exact concrete type depends on which backend is linked in:

- OpenGL: `GLuint` (a texture name).
- DirectX 11: `ID3D11ShaderResourceView*`.
- DirectX 12: descriptor handle.
- Vulkan: `VkDescriptorSet`.
- Metal: `MTLTexture` handle.

Casts between these are silent — pass an OpenGL `GLuint` to a DX11 backend and you'll either get garbage rendering or a crash on draw. Always check `imgui_impl_<backend>.h` for the concrete type the active backend expects, and centralize the cast in your asset layer (a single `MakeImTexId(GpuTexture&)` function is much easier to audit than a hundred ad-hoc casts).

## Common pitfalls (each with reproducer + fix)

1. **Forgetting `ItemSize` → next widget overlaps.** Reproducer: call `ItemAdd` without `ItemSize`. The layout cursor doesn't advance and subsequent widgets render on top of yours. Fix: always pair them — `ItemSize(size); if (!ItemAdd(bb, id)) return;`.

2. **Forgetting `ItemAdd` → no hit-testing, no nav.** `g.LastItemData` is never set, so `IsItemHovered`/`IsItemActive`/`IsItemFocused` all return `false` and keyboard nav skips your widget entirely. Fix: call `ItemAdd` before any state queries.

3. **Drawing before `ItemAdd` → wrong order, no clipping.** ImGui's clipping kicks in inside `ItemAdd`; draws emitted earlier won't respect the window's clip rect, so your widget can spill outside scroll regions or the window border. Fix: `ItemSize`, `ItemAdd`, *then* `GetWindowDrawList()->Add*`.

4. **Manual hit-testing with `IsMouseHoveringRect` → loses popup occlusion.** A popup over your widget shouldn't activate it; manual rectangle tests don't know that. Fix: `IsItemHovered()` or `ItemHoverable(bb, id, flags)` from the internal API.

5. **`GetWindowDrawList()` after `End()` → wrong list / undefined behavior.** Once you've called `ImGui::End()`, the "current window" has changed. Fix: capture the pointer inside `Begin/End`, or use `GetForegroundDrawList()` / `GetBackgroundDrawList()` for cross-cutting overlays.

## See also

- [id-stack.md](id-stack.md) — `PushID` rules and how `window->GetID(label)` hashes against the stack.
- [widget-recipes.md](widget-recipes.md) — when you want a stock widget, not a custom one.
- [pitfalls-catalog.md](pitfalls-catalog.md) — cross-cutting index of bugs and their references.
