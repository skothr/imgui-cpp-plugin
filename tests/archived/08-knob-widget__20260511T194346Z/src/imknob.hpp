// imknob.hpp -- header-only rotary-knob widget for Dear ImGui v1.92.x (docking).
//
// Follows the standard 6-step custom-widget protocol:
//   GetID -> CalcItemSize -> ItemSize -> ItemAdd -> ButtonBehavior -> DrawList.
//
// Behaviour:
//   - 270 degree sweep, indicator pointing straight up at midrange.
//   - Click-and-drag vertically to change the value; up = increase.
//   - Default sensitivity: 200 px of drag covers the full v_min..v_max range.
//     Hold Shift while dragging for 4x finer adjustment.
//   - Tooltip on hover (and during drag) shows the precise value via `format`.
//   - Optional logarithmic mapping (set KnobFlags_Logarithmic; requires v_min > 0).

#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace ImKnob {

enum KnobFlags_ : int {
    KnobFlags_None        = 0,
    KnobFlags_Logarithmic = 1 << 0,
    KnobFlags_NoTooltip   = 1 << 1,
    KnobFlags_NoInput     = 1 << 2,
};
using KnobFlags = int;

namespace detail {

inline float value_to_t(float v, float v_min, float v_max, KnobFlags flags) {
    if (flags & KnobFlags_Logarithmic) {
        const float lmin = std::log(v_min);
        const float lmax = std::log(v_max);
        return (std::log(std::max(v, v_min)) - lmin) / (lmax - lmin);
    }
    return (v - v_min) / (v_max - v_min);
}

inline float t_to_value(float t, float v_min, float v_max, KnobFlags flags) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (flags & KnobFlags_Logarithmic) {
        const float lmin = std::log(v_min);
        const float lmax = std::log(v_max);
        return std::exp(lmin + t * (lmax - lmin));
    }
    return v_min + t * (v_max - v_min);
}

}  // namespace detail

// Returns true on the frame the value changed.
inline bool Knob(const char* label,
                 float*      p_value,
                 float       v_min,
                 float       v_max,
                 float       size   = 56.0f,
                 KnobFlags   flags  = 0,
                 const char* format = "%.3f")
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext&     g     = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImGuiID id         = window->GetID(label);
    const ImVec2  label_size = ImGui::CalcTextSize(label, nullptr, true);
    const float   label_h    = label_size.x > 0.0f
                                 ? label_size.y + style.ItemInnerSpacing.y
                                 : 0.0f;

    const ImVec2 pos        = window->DC.CursorPos;
    const ImVec2 widget_sz  = ImVec2(size, size + label_h);
    const ImRect frame_bb(pos, pos + widget_sz);
    const ImRect knob_bb(pos, pos + ImVec2(size, size));

    ImGui::ItemSize(widget_sz);
    if (!ImGui::ItemAdd(frame_bb, id)) return false;

    bool hovered = false, held = false;
    ImGui::ButtonBehavior(knob_bb, id, &hovered, &held);

    // ---- Interaction ------------------------------------------------------
    // 200 px of vertical drag spans the full value range; Shift = 4x finer.
    constexpr float k_pixels_for_full_range = 200.0f;

    bool  value_changed = false;
    float t             = detail::value_to_t(*p_value, v_min, v_max, flags);
    if (held && !(flags & KnobFlags_NoInput)) {
        const float dy = -g.IO.MouseDelta.y;  // up = increase
        if (dy != 0.0f) {
            const float scale = (g.IO.KeyShift ? 4.0f : 1.0f) * k_pixels_for_full_range;
            const float new_t = std::clamp(t + dy / scale, 0.0f, 1.0f);
            const float new_v = detail::t_to_value(new_t, v_min, v_max, flags);
            if (new_v != *p_value) {
                *p_value      = new_v;
                t             = new_t;
                value_changed = true;
            }
        }
    }

    // ---- Render -----------------------------------------------------------
    const ImVec2 center  = knob_bb.GetCenter();
    const float  radius  = size * 0.5f - 2.0f;
    const float  arc_thk = std::max(2.0f, size * 0.08f);

    // 270 deg sweep, symmetric about straight-up (ImGui Y points down,
    // so straight-up is angle = -PI/2).
    constexpr float k_arc_half  = 3.0f * IM_PI / 4.0f;
    const float     angle_min   = -IM_PI * 0.5f - k_arc_half;
    const float     angle_max   = -IM_PI * 0.5f + k_arc_half;
    const float     angle_value = angle_min + t * (angle_max - angle_min);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    const ImU32 col_track = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 col_arc   = ImGui::GetColorU32(held    ? ImGuiCol_SliderGrabActive
                                              : hovered ? ImGuiCol_SliderGrab
                                                        : ImGuiCol_PlotHistogram);
    const ImU32 col_body  = ImGui::GetColorU32(hovered ? ImGuiCol_ButtonHovered
                                                       : ImGuiCol_Button);
    const ImU32 col_text  = ImGui::GetColorU32(ImGuiCol_Text);

    // Background track arc.
    dl->PathClear();
    dl->PathArcTo(center, radius, angle_min, angle_max, 48);
    dl->PathStroke(col_track, ImDrawFlags_None, arc_thk);

    // Foreground (filled) arc from min to current value.
    dl->PathClear();
    dl->PathArcTo(center, radius, angle_min, angle_value, 48);
    dl->PathStroke(col_arc, ImDrawFlags_None, arc_thk);

    // Knob body.
    const float body_r = std::max(2.0f, radius - arc_thk - 2.0f);
    dl->AddCircleFilled(center, body_r, col_body, 32);

    // Indicator line from inside the body to its outer edge.
    const ImVec2 dir(ImCos(angle_value), ImSin(angle_value));
    const ImVec2 p_inner = center + dir * (body_r * 0.30f);
    const ImVec2 p_outer = center + dir * (body_r * 0.95f);
    dl->AddLine(p_inner, p_outer, col_text, std::max(2.0f, size * 0.06f));

    // Label centred below.
    if (label_size.x > 0.0f) {
        const ImVec2 label_pos(pos.x + (size - label_size.x) * 0.5f,
                               pos.y + size + style.ItemInnerSpacing.y);
        dl->AddText(label_pos, col_text, label);
    }

    // Tooltip on hover or while dragging (BeginItemTooltip honours the
    // configured hover-delay; IsItemHovered stays true while the item is
    // active, so this also fires during a drag).
    if (!(flags & KnobFlags_NoTooltip)) {
        ImGui::SetItemTooltip(format, *p_value);
    }

    return value_changed;
}

}  // namespace ImKnob
