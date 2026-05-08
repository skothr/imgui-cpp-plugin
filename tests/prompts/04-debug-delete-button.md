First time using Dear ImGui in a real app (docking branch, v1.92.x). I've got an editor with a tab strip across the top — each tab shows a filename and has a small `×` close button next to it. Whichever tab I click `×` on, only the first tab in the list ever actually gets deleted.

The handler fires correctly (I can see it in the debugger, with the right iterator), but `delete_tab` always lands on `tabs[0]`. Here's roughly what my tab-strip submission looks like:

```cpp
void Editor::draw_tab_strip() {
    ImGui::BeginChild("##tabs", ImVec2(0, 28), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    for (auto& tab : tabs_) {
        ImU32 col = (tab.id == active_tab_id_)
                      ? IM_COL32(80, 80, 110, 255)
                      : IM_COL32(40, 40, 50, 255);
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        if (ImGui::Button(tab.name.c_str())) {
            active_tab_id_ = tab.id;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 2);
        if (ImGui::Button("×")) {
            delete_tab(tab);
        }
        ImGui::SameLine(0, 8);
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
}
```

Diagnose the root cause and show me the fix. Save your full reply (markdown, with the corrected code) to `04-debug-delete-button/response.md` (relative to your current directory).
