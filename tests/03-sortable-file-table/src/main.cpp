// main.cpp - sortable file-list table demo (Dear ImGui v1.92.x docking, GLFW + OpenGL3).

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "imscoped.hpp"

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include <string>
#include <vector>

namespace {

struct FileEntry {
    std::string name;
    std::int64_t size;     // bytes
    std::string mtime;     // "YYYY-MM-DD HH:MM"
};

std::vector<FileEntry> make_files() {
    return {
        {"CMakeLists.txt",      2'341,        "2026-05-08 12:34"},
        {"README.md",           4'096,        "2026-05-10 09:12"},
        {"main.cpp",           18'742,        "2026-05-11 08:01"},
        {"imscoped.hpp",        9'880,        "2026-04-22 17:55"},
        {"icon.png",          204'800,        "2026-03-14 11:20"},
        {"build.log",       1'048'576,        "2026-05-11 07:48"},
        {"notes.txt",             612,        "2026-02-01 22:10"},
        {"big_dataset.bin", 314'572'800,      "2026-05-09 14:33"},
        {"LICENSE",             1'084,        "2026-01-12 10:00"},
        {"changelog.md",        7'331,        "2026-05-06 19:42"},
    };
}

void sort_files(std::vector<FileEntry>& files, const ImGuiTableSortSpecs& specs) {
    if (specs.SpecsCount <= 0) return;
    const auto& s = specs.Specs[0];
    const bool asc = s.SortDirection == ImGuiSortDirection_Ascending;
    std::sort(files.begin(), files.end(), [&](const FileEntry& a, const FileEntry& b) {
        switch (s.ColumnIndex) {
            case 0: return asc ? a.name  < b.name  : a.name  > b.name;
            case 1: return asc ? a.size  < b.size  : a.size  > b.size;
            case 2: return asc ? a.mtime < b.mtime : a.mtime > b.mtime;  // ISO-ish strings sort lexically = chronologically
            default: return false;
        }
    });
}

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

void draw_file_table(std::vector<FileEntry>& files, int& selected_index) {
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Sortable
        | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_RowBg          // alternating row background stripes
        | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_Reorderable
        | ImGuiTableFlags_Borders
        | ImGuiTableFlags_Hideable;

    if (auto t = ImScoped::Table("##files", 3, table_flags, ImVec2(-FLT_MIN, -FLT_MIN))) {
        ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size",     ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupScrollFreeze(0, 1);   // freeze the header row during scroll
        ImGui::TableHeadersRow();

        if (auto* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty) {
            sort_files(files, *specs);
            specs->SpecsDirty = false;
        }

        // Virtualize with ImGuiListClipper so the loop scales to thousands of rows.
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(files.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const FileEntry& f = files[static_cast<std::size_t>(row)];
                ImScoped::ID id{row};  // stable per-row ID for selectable state

                ImGui::TableNextRow();

                // Column 0: Selectable spans the row, drives hover-highlight + click-select.
                ImGui::TableNextColumn();
                const bool is_selected = (selected_index == row);
                constexpr ImGuiSelectableFlags sel_flags =
                    ImGuiSelectableFlags_SpanAllColumns
                    | ImGuiSelectableFlags_AllowDoubleClick;
                if (ImGui::Selectable(f.name.c_str(), is_selected, sel_flags)) {
                    selected_index = row;
                }

                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(f.size));

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(f.mtime.c_str());
            }
        }
    }
}

}  // namespace

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;
    apply_window_hints();

    const float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow(
        static_cast<int>(960 * main_scale),
        static_cast<int>(600 * main_scale),
        "Sortable File Table",
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

    std::vector<FileEntry> files = make_files();
    int selected_index = -1;
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

        if (auto win = ImScoped::Window("Files")) {
            ImGui::Text("%zu entries  |  selected: %s",
                files.size(),
                (selected_index >= 0
                    ? files[static_cast<std::size_t>(selected_index)].name.c_str()
                    : "(none)"));
            ImGui::Separator();
            draw_file_table(files, selected_index);
        }

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
