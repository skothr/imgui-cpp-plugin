My Dear ImGui (docking branch) delete-confirmation modal flashes open for one frame and then immediately closes whenever I click the trigger button. The modal's title bar appears and vanishes on the same frame.

Inspector code:

```cpp
void Inspector::draw() {
    ImGui::Begin("Inspector");

    if (selected_) {
        ImGui::Text("Selected: %s", selected_->name.c_str());
        ImGui::Separator();

        // ... a bunch of property editors here, omitted for clarity ...

        if (ImGui::Button("Delete selected")) {
            pending_delete_id_ = selected_->id;
        }
        ImGui::OpenPopup("confirm-delete");
    }

    if (ImGui::BeginPopupModal("confirm-delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete this?");
        ImGui::Separator();
        if (ImGui::Button("OK")) {
            perform_delete(pending_delete_id_);
            pending_delete_id_.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pending_delete_id_.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}
```

What's wrong, why does it manifest as a one-frame flash, and what's the fix? Save your reply (markdown, with the corrected code block) to `06-debug-popup-closes/response.md` (relative to your current directory).
