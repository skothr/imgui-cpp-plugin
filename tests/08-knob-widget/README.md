# 08-knob-widget

Custom Dear ImGui rotary-knob widget demo. Three knobs (gain, pan, cutoff)
in one window. Vertical click-drag changes the value; Shift slows it down
by 10x; a tooltip shows the current value to 3 decimals on hover.

`cutoff` uses a logarithmic mapping over `[20, 20000]`; the others are linear.

Stack: C++23, GLFW + OpenGL 3, Dear ImGui v1.92.7-docking (fetched by CMake).

## Build

```
cmake -S . -B build
cmake --build build -j
./build/main
```

Stdout prints whenever any knob changes:

```
gain = 1.234
pan = -0.456
cutoff = 8123.450 Hz
```

## Where the widget lives

`src/main.cpp`, `namespace knob`. The `knob::Knob` function follows the
standard custom-widget item protocol from
`references/custom-widgets.md`: `SkipItems` -> `GetID` -> `ItemSize` ->
`ItemAdd` -> `ButtonBehavior` -> draw via `ImDrawList`. `MarkItemEdited`
is called when the value changes so `IsItemEdited()` works downstream.

Knob defaults are exposed via `knob::Config`:

- `sweep_radians` — visible arc (default 270 degrees, gap at the bottom).
- `pixels_per_full` — vertical pixels to traverse 0 -> 1 (default 200).
- `radius` — knob radius in pixels.
- `tooltip_precision` — `%.*f` precision in the tooltip.
