#pragma once

// setting_widgets — the per-type widget layer for the Settings framework. ALL the
// per-type ImGui rendering lives behind the drawWidget(T&, SettingMeta<T>&)
// overloads declared here (defined in src/settings/setting.cpp), so "how does each
// value type render" is one place to read. SettingMeta<T> carries the tunable
// metadata; the Settable concept makes an unsupported value type a COMPILE error
// at the Setting<T> / SettingGroup::add<T> call site rather than a silent no-op
// (the astrolograph weakness this design fixes).
//
// The drawWidget signatures intentionally contain NO ImGui types, so this header —
// and setting.hpp which includes it — stay ImGui-free; only setting.cpp pulls
// <imgui.h>.

#include <concepts>
#include <optional>
#include <string>
#include <vector>

#include <imtool/common/vector.hpp>   // Vec2f / Vec3f / Vec4f

namespace imtool {

// How a Setting<T> prefers to render. Auto picks a sensible default per type
// (e.g. a bounded float -> slider, an unbounded one -> drag).
enum class WidgetHint { Auto, Drag, Slider, Input, Color, Combo };

// Tunable metadata for one Setting. Display label + tooltip live on SettingBase
// (passed to the ctor); this carries only the value-shaped knobs. min/max/step are
// T-typed, so a Vec2f's bounds are a Vec2f (not a widened scalar).
template<typename T>
struct SettingMeta {
    T                        default_v {};   // reset target (the real default, not value-at-construction)
    std::optional<T>         min {};
    std::optional<T>         max {};
    T                        step {};
    T                        big_step {};
    std::string              format;          // printf-style fmt, e.g. "%.3f"
    WidgetHint               hint = WidgetHint::Auto;
    std::vector<std::string> choices;         // combo/enum label table (int settings with hint==Combo)
};

// Per-type widget renderers — render the WIDGET ONLY (the label column + PushID are
// handled by SettingBase::draw). Return true iff the value was edited this frame.
// Defined in src/settings/setting.cpp. v1 value-type set:
[[nodiscard]] bool drawWidget(bool &v,        const SettingMeta<bool> &m);
[[nodiscard]] bool drawWidget(int &v,         const SettingMeta<int> &m);
[[nodiscard]] bool drawWidget(float &v,       const SettingMeta<float> &m);
[[nodiscard]] bool drawWidget(Vec2f &v,       const SettingMeta<Vec2f> &m);
[[nodiscard]] bool drawWidget(Vec3f &v,       const SettingMeta<Vec3f> &m);
[[nodiscard]] bool drawWidget(Vec4f &v,       const SettingMeta<Vec4f> &m);
[[nodiscard]] bool drawWidget(std::string &v, const SettingMeta<std::string> &m);

// A type is Settable iff a drawWidget overload exists for it. Setting<Settable T>
// constrains on this, so an unsupported T fails to compile where it is declared.
template<typename T>
concept Settable = requires(T &v, const SettingMeta<T> &m) {
    { ::imtool::drawWidget(v, m) } -> std::same_as<bool>;
};

}  // namespace imtool
