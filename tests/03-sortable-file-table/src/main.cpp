// main.cpp — sortable file-list table (Dear ImGui + GLFW + OpenGL 3, docking).
//
// Shows the canonical BeginTable recipe:
//   - ImGuiTableFlags_Sortable + TableHeadersRow() for click-to-sort headers
//   - TableGetSortSpecs() re-queried each frame, never stored across frames
//   - Selectable(SpanAllColumns) for full-row hover/select
//   - RowBg for alternating row stripes
//   - ImGuiListClipper so the loop body scales to thousands of rows
//   - TableSetupScrollFreeze(0, 1) so the header stays visible while scrolling

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "imscoped.hpp"

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct FileEntry {
    std::string name;
    std::int64_t size = 0;        // bytes
    std::string mtime;            // "YYYY-MM-DD HH:MM"
    bool selected = false;
};

std::vector<FileEntry> make_entries() {
    return {
        {"CMakeLists.txt",        2'341,        "2026-04-21 09:14"},
        {"README.md",             1'088,        "2026-05-09 17:02"},
        {"main.cpp",             18'420,        "2026-05-10 11:48"},
        {"imscoped.hpp",          7'612,        "2026-03-02 08:30"},
        {"imgui.ini",               412,        "2026-05-10 23:55"},
        {"assets.zip",       42'118'904,        "2026-01-15 14:22"},
        {"build.log",           204'877,        "2026-05-11 06:01"},
        {"notes.txt",               973,        "2026-02-28 19:40"},
        {"video.mp4",       128'441'002,        "2025-12-19 22:10"},
        {"thumbnail.png",        91'204,        "2026-04-30 13:05"},
    };
}

// Re-sort `items` in-place per the table's current sort specs.
// `TableGetSortSpecs` returns nullptr if sortable is off; caller checks SpecsDirty.
void sort_entries(std::vector<FileEntry>& items, const ImGuiTableSortSpecs& specs) {
    std::sort(items.begin(), items.end(),
        [&specs](const FileEntry& a, const FileEntry& b) {
            for (int n = 0; n < specs.SpecsCount; ++n) {
                const ImGuiTableColumnSortSpecs& s = specs.Specs[n];
                int cmp = 0;
                switch (s.ColumnIndex) {
                    case 0: cmp = a.name.compare(b.name); break;
                    case 1:
                        cmp = (a.size < b.size) ? -1 : (a.size > b.size) ? 1 : 0;
                        break;
                    case 2: cmp = a.mtime.compare(b.mtime); break;
                    default: break;
                }
                if (cmp != 0) {
                    return (s.SortDirection == ImGuiSortDirection_Ascending)
                        ? (cmp < 0) : (cmp > 0);
                }
            }
            // Stable fallback so equal keys keep a deterministic order.
            return a.name < b.name;
        });
}

void format_size(std::int64_t bytes, char* buf, std::size_t buflen) {
    // Human-readable but stable; sort uses the raw int64, not this string.
    constexpr std::int64_t kKiB = 1024;
    constexpr std::int64_t kMiB = 1024 * 1024;
    constexpr std::int64_t kGiB = std::int64_t{1024} * 1024 * 1024;
    if (bytes >= kGiB) {
        std::snprintf(buf, buflen, "%.2f GiB", static_cast<double>(bytes) / static_cast<double>(kGiB));
    } else if (bytes >= kMiB) {
        std::snprintf(buf, buflen, "%.2f MiB", static_cast<double>(bytes) / static_cast<double>(kMiB));
    } else if (bytes >= kKiB) {
        std::snprintf(buf, buflen, "%.2f KiB", static_cast<double>(bytes) / static_cast<double>(kKiB));
    } else {
        std::snprintf(buf, buflen, "%lld B", static_cast<long long>(bytes));
    }
}

void draw_file_table(std::vector<FileEntry>& items, int& selected_index) {
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable;

    // (-FLT_MIN, 0) == fill remaining width, fill remaining parent height.
    if (auto t = ImScoped::Table("##files", 3, table_flags, ImVec2(-FLT_MIN, 0.0f))) {
        ImGui::TableSetupColumn("Name",
            ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size",
            ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
            110.0f);
        ImGui::TableSetupColumn("Modified",
            ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupScrollFreeze(0, 1);   // header row stays put during scroll
        ImGui::TableHeadersRow();

        // Sort spec lifetime is single-frame: query, sort, clear, drop the pointer.
        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
            if (specs->SpecsDirty && items.size() > 1) {
                sort_entries(items, *specs);
                specs->SpecsDirty = false;
            }
        }

        // Virtualize. With 10 items the clipper is overkill; with 10'000 it's
        // the difference between 60 fps and a perceptible hitch on scroll.
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(items.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                FileEntry& item = items[row];
                // Per-row ID by address so selection state survives sort reorders.
                ImScoped::ID id{&item};

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Selectable in the first cell with SpanAllColumns covers the
                // entire row's hover/click region while the other cells render
                // normally to its right.
                constexpr ImGuiSelectableFlags sel_flags =
                    ImGuiSelectableFlags_SpanAllColumns |
                    ImGuiSelectableFlags_AllowDoubleClick;
                if (ImGui::Selectable(item.name.c_str(), item.selected, sel_flags)) {
                    // Single-select: clear others, set this one.
                    for (auto& other : items) other.selected = false;
                    item.selected = true;
                    selected_index = row;
                }

                ImGui::TableNextColumn();
                char sizebuf[32];
                format_size(item.size, sizebuf, sizeof(sizebuf));
                // Right-align numeric column for readability.
                const float avail = ImGui::GetContentRegionAvail().x;
                const float textw = ImGui::CalcTextSize(sizebuf).x;
                if (textw < avail) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - textw));
                ImGui::TextUnformatted(sizebuf);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(item.mtime.c_str());
            }
        }
    }
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

}  // namespace

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;
    apply_window_hints();

    const float main_scale =
        ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow(
        static_cast<int>(900 * main_scale),
        static_cast<int>(560 * main_scale),
        "Sortable file table",
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

    auto entries = make_entries();
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
            ImGui::Text("%zu entries  -  click a header to sort, click a row to select",
                        entries.size());
            if (selected_index >= 0 && selected_index < static_cast<int>(entries.size())) {
                ImGui::SameLine();
                ImGui::TextDisabled("  |  selected: %s",
                                    entries[static_cast<std::size_t>(selected_index)].name.c_str());
            }
            ImGui::Separator();
            draw_file_table(entries, selected_index);
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
