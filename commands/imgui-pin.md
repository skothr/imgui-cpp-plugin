---
description: Pin Dear ImGui to a specific upstream version in the current project (CMake FetchContent / git submodule / .imgui-version marker).
argument-hint: "[version-tag]"
---

Pin the project's Dear ImGui to a specific upstream tag. If `$ARGUMENTS` is empty, default to the skill's target version (`v1.92.7-docking`) and confirm with the user before applying.

Steps:

1. Invoke the `imgui-cpp-development` skill.
2. Run `bash $CLAUDE_PLUGIN_ROOT/skills/imgui-cpp-development/scripts/locate-imgui.sh --json` to identify the current ImGui copy and how it's wired.
3. Detect the integration mode and apply the pin accordingly:
   - **CMake FetchContent**: locate `FetchContent_Declare(imgui ... GIT_TAG <old> ...)` in the user's CMakeLists.txt and update the `GIT_TAG` line to the requested tag. Show the diff before applying.
   - **Git submodule**: run `git -C <submodule-path> fetch --tags && git -C <submodule-path> checkout <tag>`. If the submodule's superproject tracks commits, run `git add <submodule-path>` and remind the user to commit the new pointer.
   - **Manual / vendored**: ask the user whether they want to re-download. Don't force a destructive change.
4. Write a `.imgui-version` marker at the project root containing the pinned tag plus a one-line comment explaining what the file is. This is project-internal metadata so future contributors / future Claude sessions can see the explicit pin without reading the build system.
5. Re-run `locate-imgui.sh` to confirm the new version is what was intended. If the version doesn't match (e.g., user hasn't re-built / re-fetched yet), tell the user explicitly that the next build will pick up the change.
6. If pinning to a different MAJOR or MINOR upstream version, recommend reading [`changelog-1.92.x.md`](../skills/imgui-cpp-development/references/changelog-1.92.x.md) for the breaking-change list before re-building.

If the user passes a tag that doesn't exist on upstream (verify with `git ls-remote --tags https://github.com/ocornut/imgui.git`), abort with a clear message — don't pin to a non-existent ref.
