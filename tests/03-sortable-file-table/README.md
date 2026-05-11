# Sortable file-list table

A small Dear ImGui demo: a table of file entries (name / size / modified) with
click-to-sort headers, hover highlight, single-select rows, alternating row
backgrounds, and a virtualized row loop so the same code scales from 10 entries
to a few thousand.

Stack: Dear ImGui v1.92.7 (docking branch), GLFW 3.4, OpenGL 3, C++23.

## Build & run

```sh
cmake -S . -B build
cmake --build build -j
./build/main
```

CMake fetches Dear ImGui and GLFW via `FetchContent` on the first configure;
no system packages required other than an OpenGL implementation.

## What's wired up

- `ImGuiTableFlags_Sortable` + `TableHeadersRow()` - click any header to sort;
  click again to toggle direction.
- `TableGetSortSpecs()` is queried inside the table body each frame and re-sorts
  the backing vector only when `SpecsDirty` is set (the spec pointer is never
  stored across frames; see `tables.md` pitfall 5).
- `Selectable(..., SpanAllColumns)` in column 0 makes the whole row the hover
  + click target. Selection is a single `int` index; ctrl/shift extension is
  left for later.
- `ImGuiTableFlags_RowBg` gives the alternating stripe.
- `ImGuiListClipper` virtualizes the row loop - only on-screen rows submit
  widgets, so swapping `make_files()` for a 100k-entry source is a drop-in.
- `TableSetupScrollFreeze(0, 1)` keeps the header row visible while scrolling.
- `ImScoped::Table` / `ImScoped::Window` / `ImScoped::ID` handle the paired
  Begin/End and Push/Pop calls - `EndTable` only runs when `BeginTable` returned
  true (the conditional-end rule).

## Notes

- Dates are stored as `"YYYY-MM-DD HH:MM"` strings; that format sorts lexically
  the same way it sorts chronologically, so the comparator can stay a plain
  string compare. If you switch to a different format, sort on a parsed
  timestamp instead.
- Sizes are `std::int64_t` bytes, displayed raw. Swap the `Text("%lld", ...)`
  for a human-readable formatter if you want KB/MB suffixes - sorting still
  happens on the underlying integer, not the displayed string.
- Multi-select, context menus, and drag-to-reorder are intentionally omitted
  for v1; the table already has `Reorderable`/`Hideable` flags on the columns
  themselves so users can rearrange the schema via right-click.
