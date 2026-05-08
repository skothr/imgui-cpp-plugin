Dear ImGui, docking branch v1.92.x. My code editor's main child panel grows about a pixel taller every frame, eventually filling the viewport. The list of code lines I'm rendering doesn't change between frames — it's literally static content. The parent is just a vanilla `Begin("Editor")` window with no special flags.

I want to understand the root cause before picking a fix — why is the layout looping like this? Then walk me through my options.

Save the full reply to `tests/05-debug-child-grows/response.md`.
