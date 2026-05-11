// main.cpp — Tools panel prototyping starter.
// Single window, no docking. GLFW + OpenGL 3 backend.

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

struct CameraState {
    float fov_degrees = 60.0f;
    float near_plane  = 0.10f;
    float far_plane   = 1000.0f;
};

constexpr CameraState k_default_camera{};

struct RenderState {
    bool   wireframe       = false;
    ImVec4 clear_color     = ImVec4(0.10f, 0.12f, 0.14f, 1.00f);
    ImVec4 demo_text_color = ImVec4(1.00f, 0.80f, 0.20f, 1.00f);
};

void print_state(const CameraState& cam, const RenderState& render) {
    std::printf("camera.fov_degrees = %.3f\n", static_cast<double>(cam.fov_degrees));
    std::printf("camera.near_plane = %.6f\n",  static_cast<double>(cam.near_plane));
    std::printf("camera.far_plane = %.3f\n",   static_cast<double>(cam.far_plane));
    std::printf("render.wireframe = %s\n",     render.wireframe ? "true" : "false");
    std::printf("render.clear_color = (%.3f, %.3f, %.3f, %.3f)\n",
                static_cast<double>(render.clear_color.x),
                static_cast<double>(render.clear_color.y),
                static_cast<double>(render.clear_color.z),
                static_cast<double>(render.clear_color.w));
    std::printf("render.demo_text_color = (%.3f, %.3f, %.3f, %.3f)\n",
                static_cast<double>(render.demo_text_color.x),
                static_cast<double>(render.demo_text_color.y),
                static_cast<double>(render.demo_text_color.z),
                static_cast<double>(render.demo_text_color.w));
    std::fflush(stdout);
}

void draw_tools_panel(CameraState& cam, RenderState& render) {
    // Single fixed top-level window — no docking.
    if (auto win = ImScoped::Window("Tools", nullptr,
                                    ImGuiWindowFlags_NoCollapse)) {
        // Header
        ImGui::TextUnformatted("Prototyping Tools");
        ImGui::Separator();

        // Camera group
        {
            ImScoped::ID camera_id{"camera"};
            ImGui::TextUnformatted("Camera");
            ImGui::SliderFloat("FOV (deg)", &cam.fov_degrees, 30.0f, 120.0f,
                               "%.1f");
            ImGui::DragFloat("Near plane", &cam.near_plane, 0.01f,
                             0.001f, cam.far_plane, "%.4f");
            ImGui::DragFloat("Far plane",  &cam.far_plane,  1.0f,
                             cam.near_plane, 100000.0f, "%.2f");
            if (ImGui::Button("Reset camera")) {
                cam = k_default_camera;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Render group
        {
            ImScoped::ID render_id{"render"};
            ImGui::TextUnformatted("Render");
            ImGui::Checkbox("Wireframe", &render.wireframe);
            ImGui::ColorEdit4("Clear color", &render.clear_color.x,
                              ImGuiColorEditFlags_NoInputs |
                              ImGuiColorEditFlags_AlphaPreview);

            // Local style-stack override: only the next Text() picks up the color.
            {
                ImScoped::StyleColor text_color{ImGuiCol_Text,
                                                render.demo_text_color};
                ImGui::TextUnformatted("Demo text (styled by local override)");
            }
            // Back to the default text color here.
            ImGui::ColorEdit4("Demo text color", &render.demo_text_color.x,
                              ImGuiColorEditFlags_NoInputs |
                              ImGuiColorEditFlags_AlphaPreview);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Apply")) {
            print_state(cam, render);
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
        static_cast<int>(900 * main_scale),
        static_cast<int>(700 * main_scale),
        "Tools panel",
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
    io.ConfigDpiScaleFonts     = true;
    io.ConfigDpiScaleViewports = true;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplGlfw_InitForOpenGL(window, /*install_callbacks=*/true);
    ImGui_ImplOpenGL3_Init(k_glsl_version);

    CameraState camera{};
    RenderState render{};

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_tools_panel(camera, render);

        ImGui::Render();

        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(render.clear_color.x * render.clear_color.w,
                     render.clear_color.y * render.clear_color.w,
                     render.clear_color.z * render.clear_color.w,
                     render.clear_color.w);
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
