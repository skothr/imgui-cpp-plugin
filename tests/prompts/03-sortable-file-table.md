I want to render a sortable file-list table in Dear ImGui. Hardcode about 8-10 file entries — each entry has a name, a size in bytes, and a modified-date string (`"2026-05-08 12:34"` style is fine). Display them in a table with columns: `Name`, `Size`, `Modified`.

Behaviour I need:

- Click any column header to sort by that column. Ascending/descending toggles on repeat click.
- Whole row should highlight on hover and select on click (single-select for now).
- Row-background striping for readability.
- Should still feel responsive if the list grew to a few thousand entries — I might add that later.

C++23, GLFW + OpenGL3, docking branch. Output under `tests/03-sortable-file-table/` with a README. No need to actually compile.
