// main.cpp — IDE-style docking editor layout.
//
// Layout on first run:
//   +-----------+-----------------------+-----------+
//   | Hierarchy |       Viewport        | Inspector |
//   |  (left)   |       (center)        |  (right)  |
//   +-----------+-----------------------+-----------+
//
// First-run default is built via DockBuilder when no existing node is found
// for the dockspace ID. On subsequent runs ImGui restores the previous layout
// from imgui.ini, so user-adjusted splits / undocks are preserved.

#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>           // DockBuilder* lives here
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "imscoped.hpp"

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>



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

// Submit the fullscreen host window that owns the dockspace + main menu bar.
// On first invocation (no existing dock node for this ID) seed a default
// 3-pane layout. On subsequent frames / runs, imgui.ini already drove the
// layout so DockBuilder is skipped.
void submit_editor_dockspace() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImScoped::StyleVar round  {ImGuiStyleVar_WindowRounding,   0.0f};
    ImScoped::StyleVar border {ImGuiStyleVar_WindowBorderSize, 0.0f};
    ImScoped::StyleVar padding{ImGuiStyleVar_WindowPadding,    ImVec2(0, 0)};

    constexpr ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoDocking  | ImGuiWindowFlags_MenuBar;

    if (auto host = ImScoped::Window("##EditorDockHost", nullptr, host_flags)) {
        if (auto mb = ImScoped::MenuBar()) {
            if (auto m = ImScoped::Menu("File")) {
                ImGui::MenuItem("New");
                ImGui::MenuItem("Open...");
                ImGui::MenuItem("Save");
                ImGui::Separator();
                ImGui::MenuItem("Quit");
            }
            if (auto m = ImScoped::Menu("Edit")) {
                ImGui::MenuItem("Undo");
                ImGui::MenuItem("Redo");
            }
            if (auto m = ImScoped::Menu("View")) {
                ImGui::MenuItem("Hierarchy");
                ImGui::MenuItem("Viewport");
                ImGui::MenuItem("Inspector");
            }
            if (auto m = ImScoped::Menu("Help")) {
                ImGui::MenuItem("About");
            }
        }

        const ImGuiID dock_id = ImGui::GetID("EditorDockspace");

        // First-run default layout: build only when no node exists yet.
        // imgui.ini restores the node on subsequent runs, so this branch
        // is skipped and user adjustments survive.
        if (ImGui::DockBuilderGetNode(dock_id) == nullptr) {
            ImGui::DockBuilderRemoveNode(dock_id);
            ImGui::DockBuilderAddNode(dock_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock_id, vp->WorkSize);

            ImGuiID left_id   = 0;
            ImGuiID right_id  = 0;
            ImGuiID center_id = 0;
            ImGui::DockBuilderSplitNode(dock_id,   ImGuiDir_Left,  0.20f, &left_id,  &center_id);
            ImGui::DockBuilderSplitNode(center_id, ImGuiDir_Right, 0.25f, &right_id, &center_id);

            ImGui::DockBuilderDockWindow("Hierarchy", left_id);
            ImGui::DockBuilderDockWindow("Viewport",  center_id);
            ImGui::DockBuilderDockWindow("Inspector", right_id);
            ImGui::DockBuilderFinish(dock_id);
        }

        ImGui::DockSpace(dock_id);
    }
}

void submit_panels() {
    if (auto w = ImScoped::Window("Hierarchy")) {
        ImGui::TextUnformatted("scene graph goes here");
    }
    if (auto w = ImScoped::Window("Viewport")) {
        ImGui::TextUnformatted("render target goes here");
    }
    if (auto w = ImScoped::Window("Inspector")) {
        ImGui::TextUnformatted("selection details go here");
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
        static_cast<int>(1600 * main_scale),
        static_cast<int>(1000 * main_scale),
        "Editor",
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

    ImGui_ImplGlfw_InitForOpenGL(window, /*install_callbacks=*/true);
    ImGui_ImplOpenGL3_Init(k_glsl_version);

    constexpr ImVec4 clear_color{0.10f, 0.12f, 0.14f, 1.00f};

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Dockspace must be submitted BEFORE the windows it can host.
        submit_editor_dockspace();
        submit_panels();

        ImGui::Render();

        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w,
                     clear_color.y * clear_color.w,
                     clear_color.z * clear_color.w,
                     clear_color.w);
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
