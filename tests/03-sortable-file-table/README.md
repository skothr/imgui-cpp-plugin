# 03 - Sortable file-list table

A Dear ImGui demo of a sortable, scrollable, full-row-selectable file table on
top of GLFW + OpenGL 3 (docking branch, C++23).

## What it does

- Hardcoded list of 10 `FileEntry` records (name, size in bytes, modified date).
- Three columns: `Name`, `Size`, `Modified` -- click any header to sort,
  click again to toggle ascending/descending.
- Whole row hover-highlights and selects on click (single-select).
- Alternating row-background stripes (`ImGuiTableFlags_RowBg`).
- Header row stays pinned while the body scrolls
  (`TableSetupScrollFreeze(0, 1)`).
- Rows submitted through `ImGuiListClipper`, so the loop body scales unchanged
  if you swap the 10 entries for 10,000.
- Columns are resizable, reorderable, and hideable via the header right-click
  menu (out of the box from the flags).

## How the sort works

The `Sortable` table flag plus `TableHeadersRow()` turns headers into sort
controls. Each frame, inside the table body:

```cpp
if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
    if (specs->SpecsDirty && items.size() > 1) {
        sort_entries(items, *specs);
        specs->SpecsDirty = false;
    }
}
```

The spec pointer's lifetime is one frame -- never store it across
`EndTable()`. Sort runs only when `SpecsDirty` is true (header click, multi-sort
change, or first frame), so on a steady-state frame this is one nullptr check.

Each row's ID is `ImScoped::ID{&item}` (address-based), so `selected` state
stays attached to the right entry after the user sorts.

## Layout

```
03-sortable-file-table/
  CMakeLists.txt
  README.md
  src/
    main.cpp
    imscoped.hpp     # RAII guards from the imgui-cpp skill
```

## Build & run

```
cmake -S . -B build
cmake --build build -j
./build/main
```

First build fetches Dear ImGui v1.92.7-docking and GLFW 3.4 via
`FetchContent`; subsequent builds are incremental. Requires a system OpenGL
implementation and a C++23 compiler (GCC 13+, Clang 16+, MSVC 19.36+).

## Notes on extending to "a few thousand entries"

The code is already clipper-driven, so the per-frame cost stays O(visible
rows) rather than O(N). Two things to watch when you scale up:

1. **Sort cost.** `std::sort` over a `std::vector<FileEntry>` of a few thousand
   is fine; if you go to hundreds of thousands, sort an index vector instead so
   the swap cost is one `int` rather than a full struct move.
2. **String storage.** The `mtime` strings are stored per row. If you load
   from a real filesystem, prefer a numeric timestamp (`std::int64_t` epoch
   seconds) and format on display -- it sorts faster and uses less memory.
