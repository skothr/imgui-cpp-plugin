I want to render a sortable file-list table in Dear ImGui. Hardcode about 8-10 file entries — each entry has `name` (string), `size` (int64 bytes), and `modified` (string, just `"2026-05-08 12:34"` style is fine). Display them in a table with these columns: `Name`, `Size`, `Modified`.

Behaviour I need:

- Click any column header to sort by that column. Ascending/descending toggles on repeat click.
- Whole row should highlight on hover and select on click (single-select for now).
- Use row-background striping for readability.
- The size column should right-align with a fixed width; the other two are stretchable.

Note in a comment near the loop where I'd plug in `ImGuiListClipper` if the file list grew to thousands of entries — I don't need it wired up for the demo, but I want the comment so I know where it goes.

C++23, GLFW + OpenGL3, docking branch. Output under `tests/03-sortable-file-table/` with the usual project layout (CMakeLists, src/main.cpp, src/imscoped.hpp, README.md). No need to actually compile.
