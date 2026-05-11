// main.cpp — Dear ImGui tools-panel prototype (GLFW + OpenGL 3).
//
// Single non-docking window. The panel exposes Camera + Render groups and an
// Apply button that prints the current state to stdout, one field per line.
//
// Adapted from the imgui-cpp skill's main_glfw_opengl3.cpp.template, with
// docking + multi-viewport disabled per the prompt ("no docking yet").

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "imscoped.hpp"

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <cstdio>

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

struct AppState {
    // Camera
    float fov_degrees = 60.0f;
    float near_plane  = 0.1f;
    float far_plane   = 1000.0f;

    static constexpr float k_default_fov  = 60.0f;
    static constexpr float k_default_near = 0.1f;
    static constexpr float k_default_far  = 1000.0f;

    // Render
    bool   wireframe   = false;
    ImVec4 clear_color{0.10f, 0.12f, 0.14f, 1.00f};
    ImVec4 demo_text_color{1.00f, 0.80f, 0.20f, 1.00f};
};

void print_state(const AppState& s) {
    std::printf("--- Apply ---\n");
    std::printf("fov_degrees     = %g\n", static_cast<double>(s.fov_degrees));
    std::printf("near_plane      = %g\n", static_cast<double>(s.near_plane));
    std::printf("far_plane       = %g\n", static_cast<double>(s.far_plane));
    std::printf("wireframe       = %s\n", s.wireframe ? "true" : "false");
    std::printf("clear_color     = (%g, %g, %g, %g)\n",
                static_cast<double>(s.clear_color.x), static_cast<double>(s.clear_color.y),
                static_cast<double>(s.clear_color.z), static_cast<double>(s.clear_color.w));
    std::printf("demo_text_color = (%g, %g, %g, %g)\n",
                static_cast<double>(s.demo_text_color.x), static_cast<double>(s.demo_text_color.y),
                static_cast<double>(s.demo_text_color.z), static_cast<double>(s.demo_text_color.w));
}

void draw_tools_panel(AppState& s) {
    if (auto w = ImScoped::Window("Tools")) {
        // Header
        ImGui::TextUnformatted("Prototype Tools");
        ImGui::Separator();

        // Camera group
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImScoped::ID group{"camera"};
            ImGui::SliderFloat("FOV (deg)", &s.fov_degrees, 30.0f, 120.0f, "%.1f");
            ImGui::DragFloat("Near plane", &s.near_plane, 0.01f, 0.001f, s.far_plane, "%.3f");
            ImGui::DragFloat("Far plane",  &s.far_plane,  1.00f, s.near_plane, 100000.0f, "%.2f");
            if (ImGui::Button("Reset camera")) {
                s.fov_degrees = AppState::k_default_fov;
                s.near_plane  = AppState::k_default_near;
                s.far_plane   = AppState::k_default_far;
            }
        }

        // Render group
        if (ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImScoped::ID group{"render"};
            ImGui::Checkbox("Wireframe", &s.wireframe);
            ImGui::ColorEdit3("Clear color", &s.clear_color.x);

            // Local style-stack override so the color picker visibly drives a
            // single line of text. The StyleColor guard pops on scope exit.
            {
                ImScoped::StyleColor text_col{ImGuiCol_Text, s.demo_text_color};
                ImGui::TextUnformatted("Demo text (color overridden via style stack)");
            }
            ImGui::ColorEdit3("Demo text color", &s.demo_text_color.x);
        }

        ImGui::Separator();
        if (ImGui::Button("Apply")) {
            print_state(s);
        }
    }
}

}  // namespace

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }
    apply_window_hints();

    const float main_scale =
        ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow(
        static_cast<int>(1100 * main_scale),
        static_cast<int>(700 * main_scale),
        "Tools panel — Dear ImGui",
        nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Docking + multi-viewport intentionally left off — prompt says "no docking yet".
    io.ConfigDpiScaleFonts     = true;
    io.ConfigDpiScaleViewports = true;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplGlfw_InitForOpenGL(window, /*install_callbacks=*/true);
    ImGui_ImplOpenGL3_Init(k_glsl_version);

    AppState state;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_tools_panel(state);

        ImGui::Render();

        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(state.clear_color.x * state.clear_color.w,
                     state.clear_color.y * state.clear_color.w,
                     state.clear_color.z * state.clear_color.w,
                     state.clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
