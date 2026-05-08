My Dear ImGui confirmation modal flashes open for one frame and then immediately closes. I see the modal title bar appear and vanish on the same frame the user clicks `Delete selected`. The relevant code:

```cpp
if (ImGui::Button("Delete selected")) {
    // … record selection, etc …
}
ImGui::OpenPopup("confirm-delete");
if (ImGui::BeginPopupModal("confirm-delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Are you sure?");
    if (ImGui::Button("OK"))     { perform_delete(); ImGui::CloseCurrentPopup(); }
    if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
    ImGui::EndPopup();
}
```

What's wrong, why does it manifest as a one-frame flash, and what's the fix? Save your reply (markdown, including the corrected code block) to `tests/06-debug-popup-closes/response.md`.
