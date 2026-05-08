I'm building an editor in C++ with Dear ImGui (docking branch, v1.92.x). My UI has a list of opened tabs, and each tab has a `Delete` button next to its name.

Bug: clicking `Delete` on tab #3 always deletes tab #0. Walking through it in the debugger, the click handler does fire and the iterator IS pointing at tab #3 — but the action lands on tab #0. Why?

```cpp
for (auto& tab : tabs) {
    if (ImGui::Button("Delete")) {
        delete_tab(tab);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(tab.name.c_str());
}
```

Diagnose the root cause and show me the fix. Save your full reply (markdown, with the corrected code) to `tests/04-debug-delete-button/response.md`.
