# ImKnob — rotary-knob widget demo

A small Dear ImGui scaffold demonstrating a header-only custom `Knob` widget
(`src/imknob.hpp`) and a three-knob audio-plugin-style demo (`src/main.cpp`).

## Files

| Path | Purpose |
|---|---|
| `src/imknob.hpp` | Header-only `ImKnob::Knob(...)` widget |
| `src/imscoped.hpp` | RAII scope guards for Begin/End pairs |
| `src/main.cpp` | Demo window: gain / pan / cutoff knobs |
| `CMakeLists.txt` | Fetches Dear ImGui v1.92.7-docking + GLFW 3.4 via FetchContent |

## Widget API

```cpp
#include "imknob.hpp"

// Returns true on the frame the value changes.
bool ImKnob::Knob(const char* label,
                  float*      p_value,
                  float       v_min,
                  float       v_max,
                  float       size   = 56.0f,
                  KnobFlags   flags  = 0,
                  const char* format = "%.3f");
```

Flags:

- `KnobFlags_Logarithmic` — log-mapped value (requires `v_min > 0`)
- `KnobFlags_NoTooltip`   — suppress the hover tooltip
- `KnobFlags_NoInput`     — display-only, ignores drag

## Interaction

- Click-and-drag vertically; up = increase, down = decrease.
- Sensitivity: ~200 pixels covers the full `v_min..v_max` range.
- Hold **Shift** while dragging for 4x finer control.
- Hover (or drag) shows a tooltip with the precise value, using `format`.
- 270 deg sweep, indicator points straight up at midrange.

## Build

```bash
cmake -S . -B build
cmake --build build -j
./build/main
```

First configure pulls Dear ImGui v1.92.7-docking and GLFW 3.4 — needs network
access. After that, `compile_commands.json` lands in `build/` for clangd.

## Implementation notes

The widget follows Dear ImGui's standard custom-widget protocol:
`GetID` -> `CalcTextSize` -> `ItemSize` -> `ItemAdd` -> `ButtonBehavior` ->
`DrawList` rendering. While the item is held, vertical mouse delta
(`io.MouseDelta.y`, negated so up = increase) is integrated into the
normalized position `t`, which is then mapped back through the linear or
logarithmic value transform. Rendering uses `PathArcTo` + `PathStroke` for
the track and filled arc, `AddCircleFilled` for the body, and `AddLine` for
the indicator.
