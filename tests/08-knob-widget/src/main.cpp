// Rotary knob widget demo — Dear ImGui + GLFW + OpenGL3 (docking branch).
//
// The knob follows the standard custom-widget item protocol:
// SkipItems guard -> GetID -> ItemSize -> ItemAdd -> ButtonBehavior -> draw.

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "imscoped.hpp"

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

namespace knob {

struct Config {
    // Sweep is centered at the bottom of the knob, opening downward.
    // 0-270 degrees means 135 degrees of dead zone at the bottom.
    float sweep_radians      = 270.0f * (std::numbers::pi_v<float> / 180.0f);
    float pixels_per_full    = 200.0f;   // vertical pixels to traverse the full range
    float radius             = 28.0f;
    int   tooltip_precision  = 3;
};

// Map a normalized t in [0,1] to/from a logarithmic value range.
float lerp_log(float v_min, float v_max, float t) {
    const float lmin = std::log(v_min);
    const float lmax = std::log(v_max);
    return std::exp(lmin + (lmax - lmin) * t);
}
float inv_lerp_log(float v_min, float v_max, float v) {
    const float lmin = std::log(v_min);
    const float lmax = std::log(v_max);
    return (std::log(v) - lmin) / (lmax - lmin);
}

// Returns true when the value changed this frame.
bool Knob(const char* label, float* p_value, float v_min, float v_max,
         bool logarithmic = false, const Config& cfg = {}) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g     = *GImGui;
    const ImGuiStyle& s = g.Style;
    const ImGuiID id    = window->GetID(label);

    const float diameter   = cfg.radius * 2.0f;
    const float label_h    = ImGui::GetTextLineHeight();
    const ImVec2 pos       = window->DC.CursorPos;
    const ImVec2 size{diameter, diameter + s.ItemInnerSpacing.y + label_h};
    const ImRect bb{pos, pos + size};
    const ImRect knob_bb{pos, pos + ImVec2{diameter, diameter}};

    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bb, id)) return false;

    // Normalize current value to t in [0,1].
    auto to_t = [&](float v) {
        v = std::clamp(v, v_min, v_max);
        return logarithmic
            ? inv_lerp_log(v_min, v_max, v)
            : (v - v_min) / (v_max - v_min);
    };
    auto from_t = [&](float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return logarithmic
            ? lerp_log(v_min, v_max, t)
            : v_min + (v_max - v_min) * t;
    };

    bool hovered = false, held = false;
    ImGui::ButtonBehavior(knob_bb, id, &hovered, &held);

    bool changed = false;
    if (held) {
        // Vertical drag: up = increase. ImGui's Y grows downward, so flip sign.
        const float dy = ImGui::GetIO().MouseDelta.y;
        if (dy != 0.0f) {
            float t = to_t(*p_value);
            const float shift_mul = ImGui::GetIO().KeyShift ? 0.1f : 1.0f;
            t -= (dy / cfg.pixels_per_full) * shift_mul;
            t = std::clamp(t, 0.0f, 1.0f);
            const float new_v = from_t(t);
            if (new_v != *p_value) {
                *p_value = new_v;
                changed  = true;
                ImGui::MarkItemEdited(id);
            }
        }
    }

    // Render -------------------------------------------------------------
    ImDrawList* dl = window->DrawList;
    const ImVec2 center = knob_bb.GetCenter();

    const ImU32 col_bg   = ImGui::GetColorU32(
        held ? ImGuiCol_FrameBgActive
             : hovered ? ImGuiCol_FrameBgHovered
                       : ImGuiCol_FrameBg);
    const ImU32 col_fill = ImGui::GetColorU32(
        held ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);
    const ImU32 col_ind  = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 col_trk  = ImGui::GetColorU32(ImGuiCol_Border);

    // Sweep math: angles measured from +X axis, CCW positive (ImGui convention).
    // We want the gap centered at the bottom (angle = +pi/2 in ImGui's
    // screen-space where Y points down). Track runs CCW from the gap's right
    // edge to its left edge.
    const float half_sweep = cfg.sweep_radians * 0.5f;
    const float pi         = std::numbers::pi_v<float>;
    const float a_start    = pi * 0.5f + half_sweep;   // CCW start (left of bottom gap when drawn)
    const float a_end      = pi * 0.5f - half_sweep + 2.0f * pi;
    // Actually: PathArcTo draws from a_min to a_max increasing. We want to
    // sweep from lower-left around the top to lower-right.
    const float a_min = pi - (pi * 0.5f - half_sweep);  // = pi/2 + half_sweep
    const float a_max = a_min + cfg.sweep_radians;
    // Disambiguate: with Y-down screen space, angle 0 = +X right,
    // angle pi/2 = down. We want our gap centered at "down" (pi/2). So the
    // arc runs from (pi/2 + half_sweep) clockwise... but PathArcTo increases
    // angle which is CCW in math but CW on screen due to Y-flip. Either way,
    // sweeping a_min -> a_min + sweep covers the visible arc.
    (void)a_start; (void)a_end;

    const float t_now = to_t(*p_value);
    const float a_val = a_min + cfg.sweep_radians * t_now;

    // Filled body
    dl->AddCircleFilled(center, cfg.radius, col_bg, 32);

    // Track (full sweep, thin)
    dl->PathArcTo(center, cfg.radius - 3.0f, a_min, a_max, 32);
    dl->PathStroke(col_trk, 0, 2.0f);

    // Value arc (filled portion)
    if (t_now > 0.0f) {
        dl->PathArcTo(center, cfg.radius - 3.0f, a_min, a_val, 32);
        dl->PathStroke(col_fill, 0, 3.0f);
    }

    // Indicator line from center to rim at the current angle
    const ImVec2 tip{
        center.x + std::cos(a_val) * (cfg.radius - 4.0f),
        center.y + std::sin(a_val) * (cfg.radius - 4.0f),
    };
    dl->AddLine(center, tip, col_ind, 2.0f);

    // Outline
    dl->AddCircle(center, cfg.radius, col_ind, 32, 1.0f);

    // Label below
    const ImVec2 label_pos{
        pos.x + (diameter - ImGui::CalcTextSize(label).x) * 0.5f,
        pos.y + diameter + s.ItemInnerSpacing.y,
    };
    dl->AddText(label_pos, ImGui::GetColorU32(ImGuiCol_Text), label);

    // Tooltip with precise value
    if (hovered || held) {
        if (auto tt = ImScoped::Tooltip()) {
            ImGui::Text("%s: %.*f", label, cfg.tooltip_precision, *p_value);
        }
    }

    return changed;
}

}  // namespace knob

namespace {

void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

constexpr const char* k_glsl_version =
#if defined(__APPLE__)
    "#version 150";
#else
    "#version 130";
#endif

void apply_window_hints() {
#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
}

}  // namespace

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;
    apply_window_hints();

    const float main_scale =
        ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow(
        static_cast<int>(640 * main_scale),
        static_cast<int>(320 * main_scale),
        "Knob widget demo",
        nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigDpiScaleFonts     = true;
    io.ConfigDpiScaleViewports = true;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(k_glsl_version);

    float gain   = 1.0f;     // [0, 2]
    float pan    = 0.0f;     // [-1, 1]
    float cutoff = 1000.0f;  // [20, 20000], log

    ImVec4 clear_color{0.10f, 0.12f, 0.14f, 1.00f};

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (auto w = ImScoped::Window("Knobs")) {
            ImGui::TextUnformatted("Drag a knob vertically. Hold Shift for fine control.");
            ImGui::Spacing();

            if (knob::Knob("gain", &gain, 0.0f, 2.0f)) {
                std::printf("gain = %.3f\n", static_cast<double>(gain));
                std::fflush(stdout);
            }
            ImGui::SameLine(0.0f, 24.0f);
            if (knob::Knob("pan", &pan, -1.0f, 1.0f)) {
                std::printf("pan = %.3f\n", static_cast<double>(pan));
                std::fflush(stdout);
            }
            ImGui::SameLine(0.0f, 24.0f);
            if (knob::Knob("cutoff", &cutoff, 20.0f, 20000.0f, /*logarithmic=*/true)) {
                std::printf("cutoff = %.3f Hz\n", static_cast<double>(cutoff));
                std::fflush(stdout);
            }
        }

        ImGui::Render();
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup);
        }

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
