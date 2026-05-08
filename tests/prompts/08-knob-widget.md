I'm prototyping an audio plugin UI in C++ and need a custom Dear ImGui rotary-knob widget. Behaviour I want:

- Circular knob, centered, drawn via `ImDrawList`. Outer ring + a tick mark indicating the current value's angular position. Span maybe 270° (e.g. 7 o'clock around to 5 o'clock).
- Click-and-drag VERTICALLY changes the value (up = increase, down = decrease). Sensitivity should feel reasonable for a typical 0..1 or 0..100 range — pick a sane default.
- Hover state: the knob highlights subtly (color change). Active state: more intense highlight.
- Tooltip on hover showing the precise current value as text (`%.3f` is fine).
- Should respect `IsItemHovered` / `IsItemActive` semantics so popups and overlays occlude correctly — don't roll your own hit testing when the framework function works.
- Should opt into ImGui's keyboard navigation (Tab to focus, arrow keys to nudge ±1% of range when focused). Use the modern `ImGuiNavMoveFlags` API, not anything deprecated in v1.92.

API:

```cpp
bool MyKnob(const char* label, float* v, float v_min, float v_max, float radius = 30.0f);
```

Returns `true` on the frame the value changed.

Provide a small demo `main.cpp` that shows three knobs in a single window: `Gain` (0..2), `Pan` (-1..1), `Cutoff` (20..20000, log-scale would be nice but linear is acceptable for the test). Print to `std::println` whenever any knob's value changes.

C++23, Linux, GLFW + OpenGL3, docking branch v1.92.x. Output everything under `tests/08-knob-widget/` (CMakeLists.txt, src/main.cpp, src/imscoped.hpp, src/myknob.hpp + .cpp if you split it, README.md with build/run instructions). No need to compile.
