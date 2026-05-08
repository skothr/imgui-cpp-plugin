---
description: Scan the current project for Dear ImGui copies; report version + branch + compile_commands.json status.
argument-hint: "[project-root]"
---

Run the imgui-locate procedure for the current project (or the path the user passed).

Steps:

1. Invoke the `imgui-cpp-development` skill so its conventions and routing are loaded.
2. Run `bash $CLAUDE_PLUGIN_ROOT/skills/imgui-cpp-development/scripts/locate-imgui.sh "$ARGUMENTS"` (or with the cwd if no argument).
3. Read the result and follow the procedure in `skills/imgui-cpp-development/references/locate-imgui.md` for the case that matched (found at expected version / found stale / multiple copies / not found).
4. If `compile_commands.json` is missing, follow up with `bash $CLAUDE_PLUGIN_ROOT/skills/imgui-cpp-development/scripts/ensure-compile-commands.sh` and report the project-appropriate snippet.
5. Summarize the findings in 4–6 lines: version, branch, path, compile_commands status, recommended next step.

Do not modify any project files as part of this command — `imgui-locate` is read-only.
