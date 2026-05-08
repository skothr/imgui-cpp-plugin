I'm prototyping an audio plugin UI in C++ and need a custom Dear ImGui rotary-knob widget. Behaviour I want:

- Circular knob, span maybe 0-270° by default
- Click-and-drag changes the value vertically. Sensitivity should feel reasonable, pick a sane default.
- Tooltip on hover showing the precise current value as text (`%.3f` is fine).

Provide a small demo `main.cpp` that shows three knobs in a single window: gain(0..2), pan(-1..1), and cutoff(20..20000, log-scale). Print to stdout whenever any knob's value changes.

C++23, Linux, GLFW + OpenGL3, docking branch v1.92.x. Output everything under `08-knob-widget/` (relative to your current directory). No need to compile.
