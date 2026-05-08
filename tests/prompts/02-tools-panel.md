Build me a small Dear ImGui tools panel I can use as a starting point for prototyping. Single window, no docking yet. The panel should have, from top to bottom:

- A header with the app title and a separator under it.
- A `Camera` group: FOV slider (degrees, 30..120), near/far plane drag-floats, "Reset camera" button.
- A `Render` group: a "Wireframe" checkbox, a clear-color color picker, and a "Demo text" line whose color I can override locally so I can see the style stack working.
- An "Apply" button at the bottom that prints the current state to the console (one field per line).

Linux, CMake, clang. Output everything under `02-tools-panel/` (relative to your current directory) with a README that has build/run instructions. I do not need you to actually invoke the compiler — just produce the working files.
