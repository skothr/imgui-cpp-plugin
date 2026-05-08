Build me a small Dear ImGui tools panel I can use as a starting point for prototyping. Single window, no docking yet. The panel should have, from top to bottom:

- A header line with the app title — bold-ish, separator under it.
- A `Camera` group with: FOV slider (degrees, 30..120), near/far plane drag-floats, "Reset camera" button.
- A `Render` group with: a "Wireframe" checkbox, a clear-color `ColorEdit3`, an `ImGuiCol_Text` color override applied to a "Demo text" line so I can confirm the style stack works.
- An "Apply" button at the bottom that prints the full current state to stdout via `std::println` (one field per line).

Linux, CMake, clang, C++23. GLFW + OpenGL3 backend. Use the bundled RAII scope guards. Output everything under `tests/02-tools-panel/` (CMakeLists.txt, src/main.cpp, src/imscoped.hpp, README.md with build/run instructions). I do not need you to actually invoke the compiler — just produce the working files.
