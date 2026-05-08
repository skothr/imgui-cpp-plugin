My Dear ImGui app has a child window that grows about a pixel taller every frame until it eventually fills the viewport. The relevant code:

```cpp
ImGui::BeginChild("##editor", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
for (auto& line : code_lines) {
    ImGui::TextUnformatted(line.c_str());
}
ImGui::EndChild();
```

`code_lines` doesn't change between frames. The parent is a normal `Begin("Editor")` window with no special flags. Docking branch v1.92.x.

What is actually happening — why does this loop in the layout system, and what are my options? I want to understand the root cause before picking a fix; please explain the feedback before showing me corrected code.

Save the full reply to `tests/05-debug-child-grows/response.md`.
