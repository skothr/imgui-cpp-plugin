# Tables

> **Load this file when:** building a data grid — sortable columns, frozen rows/columns, multi-column selectable rows, virtualized large datasets via `ImGuiListClipper`, or per-cell background colors. The `BeginTable` API is rich and pitfall-heavy; reach for it instead of nested `Columns()`.
>
> **Shape:** TLDR with the canonical sortable+scrollable+virtualized recipe at the top, followed by per-API surfaces (setup, rows, sorting, selection, virtualization, cell colors), then pitfalls and the call-flow recap. For a single-section answer, jump via the Quick navigation below.

<!-- QUICK_NAV_BEGIN -->
> **Quick navigation** (jump to a section instead of loading the whole file - `Read offset=N limit=M`):
>
> - L  36-72   TLDR — canonical sortable, scrollable, virtualized grid
> - L  73-84   Find your task
> - L  85-108  BeginTable signature and the conditional-end rule
> - L 109-134  The "Tables" section index in imgui_tables.cpp
> - L 135-157  Column setup
> - L 158-170  Per-row submission
> - L 171-182  Width policies
> - L 183-200  Table-level sizing flags
> - L 201-231  Sorting
> - L 232-249  Selection in tables
> - L 250-268  ImGuiListClipper for big tables
> - L 269-285  Per-cell background colors
> - L 286-310  Common pitfalls
> - L 311-329  Typical call-flow recap
> - L 330-333  Demo as the canonical reference
> - L 334-338  See also
<!-- QUICK_NAV_END -->








The Tables API replaces the legacy `Columns()` API for everything except the simplest two-column form. It's faster, supports sorting, freezing, scrolling, resizable/reorderable headers, and per-cell styling — but the call discipline is stricter than most ImGui APIs, and the most common bugs come from missing one of `EndTable`, the column-count contract, or the sort-spec lifetime.

## TLDR — canonical sortable, scrollable, virtualized grid

If "I want a sortable scrollable table of N items" matches the question, this is the answer. The rest of the file documents each knob the recipe uses.

```cpp
if (auto t = ImScoped::Table("##items", 3,
        ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders,
        ImVec2(-FLT_MIN, 400))) {
    ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_DefaultSort);
    ImGui::TableSetupColumn("Size",     ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupScrollFreeze(0, 1);   // freeze header row during scroll
    ImGui::TableHeadersRow();

    if (auto* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty) {
        sort_items(items, *specs);
        specs->SpecsDirty = false;
    }

    ImGuiListClipper clipper;
    clipper.Begin((int)items.size());
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            auto& item = items[row];
            ImScoped::ID id{&item};
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(item.name.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%lld", item.size);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(item.mtime.c_str());
        }
    }
}
```

Why each piece is there: `ImScoped::Table` makes `EndTable` impossible to forget on early returns; `TableSetupScrollFreeze(0, 1)` keeps the header row visible during scroll; `TableGetSortSpecs` is queried once per frame and dropped — never stored across frames; `ImGuiListClipper` skips off-screen rows so the loop scales to millions of items; `ImScoped::ID{&item}` keeps per-row state stable across reorder. For variations or to understand any individual piece, jump to the relevant section below.

## Find your task

| You want to… | Section |
|---|---|
| Set up columns and headers | Column setup |
| Decide how columns share width | Width policies; Table-level sizing flags |
| Make headers click-to-sort | Sorting |
| Click rows like a list | Selection in tables |
| Render thousands of rows | ImGuiListClipper for big tables |
| Color a cell or row | Per-cell background colors |
| Diagnose "EndTable assert" / "freeze does nothing" / "sort specs read garbage" | Common pitfalls |

## BeginTable signature and the conditional-end rule

From `imgui.h:912-913`:

```cpp
IMGUI_API bool BeginTable(
    const char*    str_id,
    int            columns,
    ImGuiTableFlags flags     = 0,
    const ImVec2&  outer_size = ImVec2(0.0f, 0.0f),
    float          inner_width = 0.0f);
IMGUI_API void EndTable();   // only call EndTable() if BeginTable() returns true!
```

This is a **conditional-end Begin**: `EndTable()` runs *only* when `BeginTable()` returned true. It's the same rule as `BeginPopup`/`EndPopup`, `BeginCombo`/`EndCombo`, etc. — and the opposite of `Begin`/`End` and `BeginChild`/`EndChild`. `ImScoped::Table` encodes this in its destructor, so you don't have to remember:

```cpp
if (auto t = ImScoped::Table("##items", 3, ImGuiTableFlags_RowBg)) {
    // ... rows ...
}   // ~Table calls EndTable() iff BeginTable() returned true
```

`outer_size = (0, 0)` means auto-fit content height; `(0, -1)` means fill remaining height of the parent; `(-FLT_MIN, 300)` means full available width and a fixed 300px scrollable height. `inner_width` is only used with `ImGuiTableFlags_ScrollX` to set the virtual width that stretch columns share.

## The "Tables" section index in imgui_tables.cpp

Per the leading comment in `imgui_tables.cpp:6-22`:

```
Index of this file:

// [SECTION] Commentary
// [SECTION] Header mess
// [SECTION] Tables: Main code
// [SECTION] Tables: Simple accessors
// [SECTION] Tables: Row changes
// [SECTION] Tables: Columns changes
// [SECTION] Tables: Columns width management
// [SECTION] Tables: Drawing
// [SECTION] Tables: Sorting
// [SECTION] Tables: Headers
// [SECTION] Tables: Context Menu
// [SECTION] Tables: Settings (.ini data)
// [SECTION] Tables: Garbage Collection
// [SECTION] Tables: Debugging
// [SECTION] Columns, BeginColumns, EndColumns, etc.
```

When you need to know what a flag actually does, jumping to the matching section in this file is faster than guessing from the name.

## Column setup

Run all `TableSetupColumn` calls *before* the first row (header or data). They register the columns; nothing renders yet.

- `TableSetupColumn(label, flags, init_width_or_weight, user_id)` (`imgui.h:926`) — declare a column. `flags` includes width-policy (`WidthFixed`/`WidthStretch`/`WidthAuto`), default-sort (`DefaultSort`, `PreferSortAscending`, `PreferSortDescending`), visibility (`DefaultHide`, `NoHide`), and behavior (`NoResize`, `NoReorder`, `NoSort`).
- `TableHeadersRow()` (`imgui.h:929`) — submit the header row built from your `TableSetupColumn` labels. Headers enable click-to-sort, drag-to-reorder, right-click context menu, and column show/hide.
- `TableSetupScrollFreeze(cols, rows)` (`imgui.h:927`) — pin the first `cols` columns and `rows` rows so they stay visible during scroll. Requires `ImGuiTableFlags_ScrollX`/`ScrollY` to do anything.

Typical setup:

```cpp
if (auto t = ImScoped::Table("##files", 3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY, ImVec2(-FLT_MIN, 400))) {
    ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Size",     ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupScrollFreeze(0, 1);   // freeze the header row
    ImGui::TableHeadersRow();
    // ... rows ...
}
```

## Per-row submission

Two row-advance functions, four valid mixing patterns, and one wrong one. Per `imgui.h:907-910`:

```
TableNextRow() -> TableSetColumnIndex(0) -> Text(...) -> TableSetColumnIndex(1) -> Text(...)  // OK
TableNextRow() -> TableNextColumn()      -> Text(...) -> TableNextColumn()      -> Text(...)  // OK
                  TableNextColumn()      -> Text(...) -> TableNextColumn()      -> Text(...)  // OK: auto-wraps to next row
TableNextRow()                           -> Text(...)                                          // NOT OK: no column selected
```

`TableNextColumn()` returns `false` for hidden/clipped columns — skip submitting that cell to save work. Use `TableSetColumnIndex(n)` when you want to skip ahead, e.g. to fill column 0 and column 2 but leave column 1 blank.

## Width policies

A column's effective width comes from the combination of its column-level flag and the table-level sizing policy.

Column-level flags (in `TableSetupColumn`):

- `ImGuiTableColumnFlags_WidthFixed` — explicit pixel width. With `init_width_or_weight = 0` it auto-fits content; with a positive value it's that many pixels (without cell padding — see pitfall #10).
- `ImGuiTableColumnFlags_WidthStretch` — share leftover space proportionally with other stretch columns. `init_width_or_weight` is the relative weight (default `1.0`).
- `ImGuiTableColumnFlags_WidthAuto` — fit to content, expand as needed. Cannot mix with stretch.

If you don't pass any width flag, the column inherits from the table-level sizing policy.

## Table-level sizing flags

Per `imgui_tables.cpp:128-133`:

- `ImGuiTableFlags_SizingFixedFit` — default columns are `WidthFixed`; each column's default width = its content width.
- `ImGuiTableFlags_SizingFixedSame` — default columns are `WidthFixed`; each column's default width = max content width across all columns.
- `ImGuiTableFlags_SizingStretchSame` — default columns are `WidthStretch` with weight = `1.0` (every column gets an equal share).
- `ImGuiTableFlags_SizingStretchProp` — default columns are `WidthStretch` with weight proportional to content (wider content gets more space). This is the default if you specify no sizing flag and no `ScrollX`.

Mental picture:

- *FixedFit* — "tight columns, sized to contents."
- *FixedSame* — "all columns the same width, sized to the widest content."
- *StretchSame* — "fill the row equally."
- *StretchProp* — "fill the row, biased toward longer content."

Mixing fixed and stretch columns is fine and idiomatic: fixed leading columns (icon, status badge) followed by a stretch trailing column (description). Mixing with `ScrollX` requires explicit `inner_width` so stretch columns have a defined total to share (`imgui_tables.cpp:135-140`).

## Sorting

Enable with `ImGuiTableFlags_Sortable` on the table and `TableHeadersRow()` to make headers clickable. ImGui tracks click order, multi-sort (with `ImGuiTableFlags_SortMulti`), and direction; you do the actual data sort when ImGui says specs changed.

`TableGetSortSpecs()` (`imgui.h:938`) returns the spec struct (`imgui.h:2232-2239`):

```cpp
struct ImGuiTableSortSpecs {
    const ImGuiTableColumnSortSpecs* Specs;
    int  SpecsCount;        // usually 1; >1 with SortMulti
    bool SpecsDirty;        // sort your data when true, then clear
};
```

The lifetime is documented in `imgui.h:938`: "Lifetime: don't hold on this pointer over multiple frames or past any subsequent call to BeginTable()." Read it, sort, clear `SpecsDirty`, drop the pointer.

```cpp
if (auto* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty) {
    std::sort(items.begin(), items.end(), [specs](const auto& a, const auto& b) {
        const auto& s = specs->Specs[0];
        const bool asc = s.SortDirection == ImGuiSortDirection_Ascending;
        switch (s.ColumnIndex) {
            case 0: return asc ? a.name < b.name : a.name > b.name;
            case 1: return asc ? a.size < b.size : a.size > b.size;
            default: return false;
        }
    });
    specs->SpecsDirty = false;
}
```

## Selection in tables

`Selectable` with `ImGuiSelectableFlags_SpanAllColumns` makes a single selectable cover the whole row's hover/click region while you still render per-column content normally. Combine with `ImGuiSelectableFlags_AllowDoubleClick` for double-click-to-open semantics; manage the selection state yourself.

```cpp
ImGui::TableNextRow();
ImGui::TableNextColumn();
if (ImGui::Selectable(item.name.c_str(), item.selected,
        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
    if (ImGui::IsMouseDoubleClicked(0)) open(item);
    else                                 toggle_select(item, ImGui::GetIO().KeyCtrl);
}
ImGui::TableNextColumn(); ImGui::Text("%lld", item.size);
ImGui::TableNextColumn(); ImGui::TextUnformatted(item.mtime.c_str());
```

The `Selectable` goes in the *first* column of the row; remaining `TableNextColumn` calls fill the row to the right.

## ImGuiListClipper for big tables

For tables with thousands of rows, virtualize with `ImGuiListClipper`. The clipper uses scroll position and a known row count to compute which rows are visible, and only your loop body runs for those.

```cpp
ImGuiListClipper clipper;
clipper.Begin((int)items.size());
while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        ImScoped::ID id{row};
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(items[row].name.c_str());
        ImGui::TableNextColumn(); ImGui::Text("%lld", items[row].size);
    }
}
```

Don't combine the clipper with `Selectable(... SpanAllColumns)` if you also want keyboard nav across all rows — the clipper hides off-screen items from the focus path. The demo's "Tables" section has a worked example.

## Per-cell background colors

`TableSetBgColor(target, color, column_n)` (`imgui.h:946`) recolors a specific cell, row, or column. Targets (`imgui.h` near `2225`):

- `ImGuiTableBgTarget_RowBg0`, `RowBg1` — alternate row background layers.
- `ImGuiTableBgTarget_CellBg` — single cell.

```cpp
ImGui::TableNextRow();
if (item.error) {
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(180, 30, 30, 64));
}
ImGui::TableNextColumn(); ImGui::TextUnformatted(item.name.c_str());
```

Call after `TableNextRow()`/`TableSetColumnIndex()` so the cell context is established.

## Common pitfalls

**1. Missing `EndTable` because `BeginTable` returned false.** Symptom: assertions or crashes deep in the next frame's draw. Cause: someone wrapped only the success path in `if (BeginTable(...))` but called `EndTable()` outside. Use `ImScoped::Table`; the destructor runs `EndTable` only on the true case automatically.

**2. Wrong column count.** `BeginTable("...", 3)` followed by 4 `TableSetupColumn` calls or 5 cells per row produces undefined behavior. `TableNextColumn()` returns false past the last column, so cells silently disappear. Match `columns` to your actual layout exactly; if the count is computed, assert it.

**3. `Selectable` doesn't span the row.** Symptom: clicking only "selects" when the cursor is over the first cell's text. Cause: forgot `ImGuiSelectableFlags_SpanAllColumns`. Add it.

**4. Frozen rows not visible.** Symptom: `TableSetupScrollFreeze(0, 1)` does nothing, header row scrolls away with the data. Cause: `ImGuiTableFlags_ScrollY` not set — freezing is a *scroll behavior*, and there's no scrolling without the flag. Add `ScrollY` and an `outer_size.y > 0`.

**5. Sort specs accessed across frames.** Symptom: `Specs` pointer reads garbage on the second frame. Cause: storing `ImGuiTableSortSpecs*` in a member or static. Re-query each frame inside the table's body; never hold across `EndTable()` or `BeginTable()` calls.

**6. `outer_size` confusion.**
- `(0, 0)` — auto-fit content. Table grows with rows.
- `(0, -1)` — fill remaining height of parent. Useful inside a child window.
- `(-FLT_MIN, 300)` — full available width, fixed 300px height. Use this for scrollable tables; it's the documented idiom in the demo.

**7. Cell padding vs column width.** Per `imgui_tables.cpp:125`: "Widths are specified *without* CellPadding. If you specify a width of 100.0f, the column will be cover (100.0f + Padding * 2.0f)". Symptom: a 100px-wide image clips because you set `init_width = 100`. Add the padding back, or shrink the image, or query `GetContentRegionAvail().x` inside the cell.

**8. Context menu broken.** `ImGuiTableFlags_ContextMenuInBody` requires a header row (or at least `TableSetupColumn`) to know which column was right-clicked. Add `TableHeadersRow()` even if you style it away.

**9. Mixed StretchProp + ScrollX without `inner_width`.** Stretch columns need a defined total width to share; with horizontal scroll the parent width isn't authoritative. Pass `inner_width` to `BeginTable` explicitly.

**10. Frozen columns flicker or misalign when horizontally scrolling.** Symptom: the first column shifts by a pixel as you scroll. Cause: the frozen column has an auto width that recomputes per-frame from clipped content. Fix: give frozen columns an explicit width via `TableSetupColumn(label, ImGuiTableColumnFlags_WidthFixed, 120.0f)` so their layout doesn't depend on visible-content measurements.

## Typical call-flow recap

The full flow as documented at `imgui_tables.cpp:36-71`:

```
BeginTable()                user begin
  TableSetupColumn()         user, optional, repeat per column
  TableSetupScrollFreeze()   user, optional
  TableUpdateLayout()        internal, auto-called by first TableNextRow / TableHeadersRow
  TableHeadersRow()          user, optional but enables sort + context menu
  TableGetSortSpecs()        user, query inside the table
  TableNextRow()             user, per row
  TableNextColumn() / TableSetColumnIndex()
    ... user content ...
EndTable()                   user end (only if BeginTable returned true)
```

The two ordering rules that matter at the top of this list: column setup and freeze setup *must* come before the first `TableNextRow()` or `TableHeadersRow()` (because the first row triggers `TableUpdateLayout()` internally and freezes the schema). Sort spec reads can happen anywhere inside the table body, but conventionally sit right after `TableHeadersRow()` so you sort the data once before submitting rows.

## Demo as the canonical reference

`imgui_demo.cpp` near line 5796 ("Tables & Columns") contains worked examples for every flag combination this reference describes — basic patterns, borders/backgrounds, resizable/stretch, sorting, frozen rows, clipper integration, multi-row selection, per-cell colors, and the angled headers variant. When a flag combination produces unexpected layout, finding the matching demo subsection and diffing against your code is faster than reading the source. Submit `ImGui::ShowDemoWindow()` once per frame to bring it up live.

## See also

- [widget-recipes.md](widget-recipes.md) — `Selectable`, drag-drop, and the other building blocks tables compose with.
- [id-stack.md](id-stack.md) — why `ImScoped::ID{&item}` per row matters when cells contain interactive widgets.
- [pitfalls-catalog.md](pitfalls-catalog.md) — cross-cutting index of "X breaks because Y" symptoms.
