# Layout and sizing

> **Load this file when:** sizing or scrolling behaves unexpectedly — windows or child frames that won't shrink/grow as expected, "child window keeps growing one pixel each frame", scrollbar appearing or vanishing surprisingly, or any `SetNextWindow*` call seemingly being ignored.

<!-- QUICK_NAV_BEGIN -->
> **Quick navigation** (jump to a section instead of loading the whole file - `Read offset=N limit=M`):
>
> - L  23-49   How sizing actually decides
> - L  50-62   Auto-fit on first frame, ini state thereafter
> - L  63-72   Why your `SetNextWindowSize` is being ignored
> - L  73-103  `BeginChild` sizing modes
> - L 104-166  The canonical "child frame keeps growing every frame" pattern
> - L 167-176  Scrollbar behavior in `BeginChild`
> - L 177-200  Style fields that affect sizing
> - L 201-220  Layout-query functions
> - L 221-246  `ImGuiSizeCallback` for custom constraints
> - L 247-251  See also
<!-- QUICK_NAV_END -->



This reference covers how Dear ImGui actually decides window and child sizes, why your `SetNextWindowSize` may be silently ignored, the canonical fix for the "child frame keeps growing" pattern, and the layout-query helpers you reach for when laying widgets out by hand. The deepest pitfalls cluster around two facts: sizing is a *negotiation* between code, the `.ini` file, and content; and `BeginChild` with `AutoResize` flags creates a feedback loop the moment content depends on size.

## How sizing actually decides

A window's size on any given frame is the result of three inputs settled in this order:

1. **`.ini` saved state.** On startup, ImGui parses the imgui.ini file (or `io.IniFilename` location). If the user resized your window in a previous session, that size is loaded before any of your code runs.
2. **Your `SetNextWindow*` calls,** consumed by `Begin()`. These override the ini state — but only if you used a `cond` that lets them (see below).
3. **Auto-fit to content** if neither of the above pinned a size. First-frame auto-fit is also what populates the ini if the user never resizes manually.

The order of operations within a single frame is:

```
SetNextWindowPos / Size / Constraints / Collapsed     // populate g.NextWindowData
Begin("MyWindow")                                     // consume g.NextWindowData, resolve final pos/size
... submit widgets ...
End()
```

`SetNextWindow*` writes into `g.NextWindowData`; `Begin()` reads and clears it. That is the entire mechanism. Get the order wrong and your override applies to whichever `Begin()` *next* receives that state — almost certainly not the one you wanted.

The header is explicit on this. From `imgui.h:483-487`:

> `SetNextWindowPos(...)` — set next window position. **call before Begin()**.
> `SetNextWindowSize(...)` — set next window size. **set axis to 0.0f to force an auto-fit on this axis. call before Begin()**.
> `SetNextWindowSizeConstraints(...)` — set next window size limits. Use 0.0f or FLT_MAX if you don't want limits.

There is no runtime assertion catching "called after Begin" — the call simply primes state for the next consumer. That makes it a silent bug, which is why it earns its own section below.

## Auto-fit on first frame, ini state thereafter

The first time your code calls `Begin("MyWindow")` and there is no `[Window][MyWindow]` block in imgui.ini, ImGui auto-fits the window to the content you submit during that frame. The resulting size is what gets saved to ini if/when settings persist. On every subsequent run, the ini block wins — until either you delete it, or the user resizes the window (which updates the ini), or your code passes `ImGuiCond_Always` to force the size each frame.

The `ImGuiCond` enum (per `imgui.h:2076` and the comments around the `SetWindow*` family) gives you four common policies:

- `ImGuiCond_Always` — apply every frame. The strongest override, useful for windows whose size is genuinely controlled by your code (a status bar pinned to the bottom, a fixed-size tool window).
- `ImGuiCond_Once` — apply on the first call only, regardless of ini state. Rarely the right choice.
- `ImGuiCond_FirstUseEver` — apply only if there is no saved ini state. Suggests "this is a sensible default; let the user customize." This is the right default for most application windows.
- `ImGuiCond_Appearing` — apply each time the window transitions from hidden to shown. Useful for popups and modals you want to recenter on each open.

The demo's "ShowExampleAppConstrainedResize" passes `ImGuiCond_FirstUseEver` for exactly that reason — it picks an initial size but defers to user resizing. Match that pattern unless you have a concrete reason not to.

## Why your `SetNextWindowSize` is being ignored

Three causes, in order of frequency:

1. **The `.ini` has the size pinned and you used `ImGuiCond_FirstUseEver`.** This is the most common one. `FirstUseEver` only applies the first time the window is created with no ini entry. Once the user resizes manually (or you save a session), your code's value is shadowed forever. To force every frame, use `ImGuiCond_Always`. To restore the original behavior, delete the relevant `[Window][...]` block in imgui.ini.
2. **Called after `Begin()`.** `g.NextWindowData` is consumed at the top of `Begin`, so calling `SetNextWindowSize` between `Begin` and `End` queues it for the *next* `Begin` call. Move the call up.
3. **`ImGuiWindowFlags_AlwaysAutoResize` is set on the window.** From `imgui.h:1220` — "Resize every window to its content every frame." Explicit size becomes a one-frame visual flicker at most; auto-fit wins.

If none of those apply, run Demo > Tools > ID Stack Tool to confirm you're addressing the same window you think you are (a duplicate `Begin("X")` somewhere else in the codebase is rarer but happens).

## `BeginChild` sizing modes

`BeginChild` signature, per `imgui.h:463`:

```cpp
bool BeginChild(const char* str_id,
                const ImVec2& size = ImVec2(0, 0),
                ImGuiChildFlags child_flags = 0,
                ImGuiWindowFlags window_flags = 0);
```

The size vector encodes mode per-axis. From `imgui.h:452-455`:

> Manual sizing (each axis can use a different setting e.g. `ImVec2(0.0f, 400.0f)`):
>  `== 0.0f`: use remaining parent window size for this axis.
>  `> 0.0f`: use specified size for this axis.
>  `< 0.0f`: right/bottom-align to specified distance from available content boundaries.

So:

- `ImVec2(0, 0)` — fill the parent's remaining space on both axes. This is the default and almost always what you want for the "main content" child of a window.
- `ImVec2(W, H)` with both positive — fixed size in pixels. Most predictable. Content that overflows scrolls (or clips, depending on flags).
- `ImVec2(-FLT_MIN, -FLT_MIN)` — fill *all* remaining space exactly, including any sub-pixel residual that `ImVec2(0, 0)` rounds away. Reach for this when you want a child to butt up against the parent edge with zero gap. Padding bugs at the bottom-right corner of your layout are usually a missing `-FLT_MIN`.
- `ImVec2(0, 200)` (mixed) — auto-fill width, fixed height. Common for a fixed-height log pane atop a flexible-height editor.

`ImGuiChildFlags_AutoResizeX` / `AutoResizeY` (from `imgui.h:1269-1270`) make the child measure its content along that axis instead of taking a parent-allocated size. This is useful for content-fit panels but creates a problem the upstream comment is blunt about (`imgui.h:457`):

> Combining both `ImGuiChildFlags_AutoResizeX` *and* `ImGuiChildFlags_AutoResizeY` defeats purpose of a scrolling region and is **NOT recommended**.

If both axes auto-resize, there's no axis along which content can overflow into a scrollbar — the child just grows. Combine that with content whose size depends on the child (wrapping text, item-per-line lists), and you get a feedback loop.

## The canonical "child frame keeps growing every frame" pattern

This is the user-flagged pain area. The reproducer:

```cpp
// WRONG — child grows whenever content grows, never shrinks back.
if (auto c = ImScoped::Child("##log",
                             ImVec2(0, 0),
                             ImGuiChildFlags_AutoResizeY)) {
    for (auto& line : log_lines) {
        ImGui::TextUnformatted(line.c_str());
    }
}
```

Every appended line increases the measured content height; the child grows; the parent window auto-fits to it; next frame, more lines arrive and the cycle repeats. There's nothing to bound it.

Three fixes, each appropriate in different situations:

**Fix 1 — Pin the height explicitly.** The simplest, and right whenever you have a sensible default:

```cpp
constexpr float k_log_height = 240.0f;
if (auto c = ImScoped::Child("##log", ImVec2(0, k_log_height),
                             ImGuiChildFlags_Borders)) {
    for (auto& line : log_lines) {
        ImGui::TextUnformatted(line.c_str());
    }
}
```

Fixed height, content scrolls past it. Use this when "the log pane is always 240 px tall" is a fine answer.

**Fix 2 — `SetNextWindowSizeConstraints` to cap growth.** Right when you want the child to *expand with content* up to a limit, then scroll:

```cpp
const float min_h = ImGui::GetTextLineHeightWithSpacing();
const float max_h = ImGui::GetTextLineHeightWithSpacing() * 12;
ImGui::SetNextWindowSizeConstraints(ImVec2(0, min_h), ImVec2(FLT_MAX, max_h));
if (auto c = ImScoped::Child("##log", ImVec2(-FLT_MIN, 0),
                             ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY)) {
    for (auto& line : log_lines) {
        ImGui::TextUnformatted(line.c_str());
    }
}
```

The `-FLT_MIN` width fills the parent; the `AutoResizeY` measures content height; the constraint clips the measurement at 12 lines. From `imgui.h:1256` — "May be combined with `SetNextWindowSizeConstraints()` to set a min/max size for each axis." This is what `Demo > Child > Auto-resize with Constraints` (in `imgui_demo.cpp:4525-4540`) uses.

**Fix 3 — Make the child resizable; let the user own the size.** Right when no single value is correct and the user benefits from dragging the boundary:

```cpp
const float default_h = ImGui::GetTextLineHeightWithSpacing() * 8;
if (auto c = ImScoped::Child("##log", ImVec2(-FLT_MIN, default_h),
                             ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY)) {
    for (auto& line : log_lines) {
        ImGui::TextUnformatted(line.c_str());
    }
}
```

`ResizeY` (`imgui.h:1268`) lets the user drag the bottom border. The child still has a definite size each frame — it's just chosen by the user, not your code. Double-clicking the resize border restores auto-fit. Note that `ResizeX/ResizeY` enables ini saving for child sizes (per the same header comment), so the user's choice persists.

## Scrollbar behavior in `BeginChild`

Vertical scrolling activates automatically whenever content extends past the child's available height. Horizontal scrolling does *not* — it requires `ImGuiWindowFlags_HorizontalScrollbar` in the `window_flags` argument (the fourth parameter, not the third — that's `ImGuiChildFlags`). From `imgui.h:1225`:

> `ImGuiWindowFlags_HorizontalScrollbar` — Allow horizontal scrollbar to appear (off by default). You may use `SetNextWindowContentSize(ImVec2(width,0.0f));` prior to calling `Begin()` to specify width.

Without that flag, content wider than the child is simply clipped on the right. Conversely, `ImGuiWindowFlags_NoScrollbar` suppresses both scrollbars; content that would scroll instead clips silently.

Do not enable `AutoResizeX` and `HorizontalScrollbar` on the same axis — there's nothing to scroll into when the child grows to fit content. Same warning for `AutoResizeY` plus vertical scrolling. Pick auto-resize *or* scroll, not both, per axis.

## Style fields that affect sizing

The defaults in `ImGuiStyle::ImGuiStyle()` at `imgui.cpp:1488-1556`:

- `WindowPadding = ImVec2(8,8)` — space between window border and content. Adds to window size when auto-fitting.
- `FramePadding = ImVec2(4,3)` — padding inside framed widgets (buttons, sliders, inputs). Controls vertical alignment of text against framed items; use `AlignTextToFramePadding()` to match.
- `ItemSpacing = ImVec2(8,4)` — gap between items. The horizontal component is the gap after `SameLine()`; the vertical component is the gap between consecutive lines.
- `ItemInnerSpacing = ImVec2(4,4)` — gap *within* a composed widget (slider and its label, drag and its arrows).
- `IndentSpacing = 21.0f` — horizontal offset added by `Indent()` and `TreeNode`. Comment notes "Generally == (FontSize + FramePadding.x*2)".
- `ScrollbarSize = 14.0f` — width of the vertical scrollbar (or height of the horizontal one). Subtracted from `GetContentRegionAvail().x` when a vertical scrollbar is visible.

When laying widgets out by hand and the result is "almost right but two pixels off," it is usually one of these. Push them via `ImScoped::StyleVar` for a scoped change:

```cpp
{
    ImScoped::StyleVar pad{ImGuiStyleVar_WindowPadding, ImVec2(2, 2)};
    if (auto w = ImScoped::Window("Tight")) {
        // tighter padding only inside this scope
    }
}
```

Note `WindowPadding` is consulted by `Begin` itself (line `imgui.cpp:8089`), so the push has to occur *before* `Begin`, not inside the window's body. The same goes for any style var that influences window decoration (`WindowRounding`, `WindowBorderSize`, `ScrollbarSize`).

## Layout-query functions

Reach for these to size widgets relative to the available area rather than hard-coding pixels:

- **`GetContentRegionAvail()`** — available space from the cursor to the right/bottom edge of the content region. Already accounts for visible scrollbars. Use for "fill remaining width" patterns: `Button("Apply", ImVec2(GetContentRegionAvail().x, 0))`.
- **`GetWindowSize()`** — full window size including title bar, borders, scrollbars. Less useful for widget layout — almost always you want `GetContentRegionAvail` instead.
- **`GetCursorScreenPos()`** — current draw position in *screen coordinates*. Pair with `ImDrawList` calls (which take screen coords). Prefer this over the deprecated `GetCursorPos`/`GetWindowContentRegionMin` pair.
- **`CalcTextSize(text, text_end, hide_text_after_double_hash, wrap_width)`** — measure rendered text in pixels. Useful for centering, right-aligning, or pre-allocating room for the longest of a set of strings.

A common sizing recipe — make a `DragFloat` take exactly half the remaining width:

```cpp
{
    ImScoped::ItemWidth iw{ImGui::GetContentRegionAvail().x * 0.5f};
    ImGui::DragFloat("##value", &f);
}
```

The legacy `GetWindowContentRegionMin` / `GetWindowContentRegionMax` pair is discouraged in current code — they return window-local coordinates and predate the cleaner `GetCursorScreenPos` + `GetContentRegionAvail` pair. The FAQ specifically recommends the latter.

## `ImGuiSizeCallback` for custom constraints

`SetNextWindowSizeConstraints` accepts a callback for non-trivial constraints — aspect-ratio-locked windows, snap-to-grid sizing, max-content-rows. Signature from `imgui.h:292`:

```cpp
typedef void (*ImGuiSizeCallback)(ImGuiSizeCallbackData* data);
```

The callback writes `data->DesiredSize` to enforce the constraint. Aspect-ratio example, lifted from the demo:

```cpp
static void AspectRatio16x9(ImGuiSizeCallbackData* data) {
    const float ar = *static_cast<float*>(data->UserData);
    data->DesiredSize.y = (float)(int)(data->DesiredSize.x / ar);
}

float ar = 16.0f / 9.0f;
ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, FLT_MAX),
                                    AspectRatio16x9, &ar);
if (auto w = ImScoped::Window("Preview")) {
    // window is now constrained to 16:9 regardless of how the user drags it
}
```

Per `imgui.h:2784-2785` — "For basic min/max size constraint on each axis you don't need to use the callback! The `SetNextWindowSizeConstraints()` parameters are enough." Use the callback only when the constraint is computed.

## See also

- [id-stack.md](id-stack.md) — when "my window won't size" turns out to be two windows colliding on the same name.
- [pitfalls-catalog.md](pitfalls-catalog.md) — cross-cutting index of layout, ID, and lifecycle bugs.
- [frame-loop.md](frame-loop.md) — Begin/End asymmetry and the per-frame ordering that determines when `SetNextWindow*` is consumed.
