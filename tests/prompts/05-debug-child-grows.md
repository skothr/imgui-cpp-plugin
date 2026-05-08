My code-editor panel in Dear ImGui (docking branch v1.92.x) has a child window that grows about a pixel taller every frame, eventually filling the viewport. The list of code lines I'm rendering doesn't change between frames — it's literally static content. The parent is a normal `Begin("Editor")` window with no special flags.

Here's the relevant submission:

```cpp
void Editor::draw_code_panel() {
    ImGui::Text("%s", current_file_.c_str());
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 22, 28, 255));
    ImGui::BeginChild("##editor", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    {
        for (size_t i = 0; i < code_lines_.size(); ++i) {
            ImGui::TextColored(line_color(i), "%4zu", i + 1);   // gutter line numbers
            ImGui::SameLine();
            ImGui::TextUnformatted(code_lines_[i].c_str());
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Text("%zu lines", code_lines_.size());
}
```

I want to understand the root cause before picking a fix — why does the layout loop like this even though the content is static? Then walk me through my options with their tradeoffs.

Save your full reply to `05-debug-child-grows/response.md` (relative to your current directory).
