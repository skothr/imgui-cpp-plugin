> **Load this file when:** the skill activates for the first time in a session — to find Dear ImGui in the user's project, identify its version + branch, and (if it's missing) help install it. Also load this when the user explicitly asks `/imgui-locate`, `/imgui-pin`, or some variant of "what version of ImGui do we have?"

This is the skill's **first action**. Subsequent guidance is grounded in whatever ImGui copy actually exists in the user's tree, not in training-data memory of older releases. The `imgui-cpp-development` skill targets v1.92.7-docking, but old projects pinned to v1.89 / v1.90 / v1.91 are everywhere — recommendations differ between them (v1.92 reworked the font system, dropped some legacy macros, and changed several public-API defaults).

## How to run it

The skill ships a script that does the scan, the parsing, and the reporting:

```bash
bash skills/imgui-cpp-development/scripts/locate-imgui.sh [project-root]
# or
bash skills/imgui-cpp-development/scripts/locate-imgui.sh --json [project-root]
```

`project-root` defaults to `$PWD`. Pass `--json` if you want machine-readable output (the slash commands and hooks consume that). Pass `--quiet` to suppress informational logs.

When you're acting as the model, run the human-readable form once at session start and remember its findings. Re-run only when the project tree has changed (a new build, a `FetchContent_Populate`, a submodule sync) or the user explicitly asks.

## What the script reports

For each Dear ImGui copy it finds:

- The path to `imgui.h`.
- `IMGUI_VERSION` — e.g. `1.92.7`.
- `IMGUI_VERSION_NUM` — e.g. `19270`. Compare numerically to gate version-conditional advice.
- The branch heuristic: `docking` (defines `IMGUI_HAS_DOCK`) vs `master` (does not).

It also reports whether `compile_commands.json` exists at the project root or under `build/`. That file is what gates clangd-driven LSP navigation; if it's missing, recommend running `scripts/ensure-compile-commands.sh` to get a project-appropriate snippet for emitting it.

## Roots the script scans

Search starts from `project-root` and probes these directories (each may contain `imgui.h`):

```
./                  (the project itself bundles ImGui)
imgui/
third_party/imgui/  third_party/Dear_Imgui/  third_party/dear_imgui/
external/imgui/
vendor/imgui/
deps/imgui/
lib/imgui/   libs/imgui/
src/imgui/   src/external/imgui/   src/third_party/imgui/
ext/imgui/
subprojects/imgui/   (Meson)
submodules/imgui/
build/_deps/imgui-src/   _deps/imgui-src/   out/build/_deps/imgui-src/   (CMake FetchContent)
```

It then runs a bounded `find -maxdepth 6` sweep to catch unusual locations (vcpkg / Conan caches, deeply-nested monorepos). `node_modules/` and `.git/` are excluded.

Multiple matches are reported — large projects sometimes have ImGui copies in different states (e.g., a vendor checkout AND a FetchContent build copy). Use the version + path to pick which one is the "live" copy you should target.

## When ImGui is missing

The script exits with code `2` and prints three concrete options. As the model you should ask which the user prefers, then run / show the matching snippet:

### Option 1: CMake + FetchContent (recommended for most projects)

```cmake
include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.7-docking
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)
```

Then build the 5 ImGui translation units + 2 backend cpps as a static library — the bundled `assets/CMakeLists-glfw-opengl3.txt.template` does this end-to-end. Copy that template rather than retyping.

### Option 2: Git submodule

```bash
git submodule add https://github.com/ocornut/imgui.git third_party/imgui
git -C third_party/imgui checkout v1.92.7-docking
```

Pin the tag explicitly so re-clones land on the exact commit. Submodule mode means the source lives in your repo tree and shows up in your IDE's project view; FetchContent hides it under `build/_deps/`.

### Option 3: Manual download

Download `https://github.com/ocornut/imgui/archive/refs/tags/v1.92.7-docking.tar.gz`, extract into `third_party/imgui/`, and add the source files to your build manually. Use this only when build-system integration isn't an option (rare).

## When ImGui is present but stale

The script flags any copy with `IMGUI_VERSION_NUM < 19270` (anything pre-v1.92.7). Don't auto-upgrade — ImGui major-version bumps tend to deprecate legacy keyio macros and rework systems (the v1.92 font rework is a notable example). The right move:

1. Tell the user the current version + the skill's target.
2. Point at [changelog-1.92.x.md](changelog-1.92.x.md) for the breaking-change list.
3. Offer to bump the pinned tag and re-build, with the explicit note that they should re-test their app.

## Compile-commands check

The script reports whether `compile_commands.json` is present. The companion script `scripts/ensure-compile-commands.sh` detects the user's build system (CMake, Meson, Bazel, Premake, raw Makefile) and emits the right snippet to enable it. Run that as a follow-up if LSP navigation is part of the upcoming work — see [lsp-navigation.md](lsp-navigation.md) for what becomes possible once `compile_commands.json` is wired up.

## Pinning a version

If the user wants to pin to a specific upstream version (and not just track whatever the existing project ships), this is what `/imgui-pin` does. Procedure:

1. Run `locate-imgui.sh` to confirm the current copy and version.
2. Edit the `GIT_TAG` line in the project's `FetchContent_Declare` (CMake) or run `git -C <submodule> checkout <tag>` (submodule). Don't modify upstream source files — that's how silent-merge bugs happen.
3. Re-run `locate-imgui.sh` to confirm the new version is what was intended.
4. (Optional) Write a `.imgui-version` file at the project root containing the intended tag, so future sessions / contributors have an explicit pin separate from the build system.

## Output flow you (the model) should follow

When the skill triggers and you run the script:

1. If exactly one copy is found at the expected version → record it, move on to the user's actual question.
2. If multiple copies are found → list them, ask which is the live one, record that.
3. If found but stale → tell the user, point at the changelog ref, ask whether to upgrade now or later.
4. If not found → walk the user through one of the three install options based on their build system. Don't proceed with code recommendations until ImGui is present.

## See also

- [lsp-navigation.md](lsp-navigation.md) — once located + `compile_commands.json` is wired up, navigate ImGui's monofiles via clangd.
- [bootstrap.md](bootstrap.md) — full project-from-zero scaffolding.
- [changelog-1.92.x.md](changelog-1.92.x.md) — what changed if you upgrade.
