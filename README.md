# claude-imgui-cpp

A Claude Code plugin for working with [Dear ImGui](https://github.com/ocornut/imgui) in modern C++23. Pinned upstream target: **v1.92.7-docking**.

The plugin ships:

- **`imgui-cpp-development` skill** — auto-triggers on ImGui-related work; routes to focused sub-docs covering bootstrap, frame loop, ID stack, layout/sizing, styling/fonts/DPI, docking + viewports, tables, modals, widget recipes, custom widgets, the v1.92 changelog, RAII scope guards, headless testing, LSP navigation, and the OpenGL3+GLFW backend. Each sub-doc is independently loadable, so the model loads only what the task needs.
- **Slash commands** — `/imgui-bootstrap` (scaffold a new project), `/imgui-locate` (find ImGui in the current project tree), `/imgui-review` (audit current code for known pitfalls), `/imgui-pin` (pin a specific upstream version).
- **Hooks** — non-blocking `PostToolUse` lints that flag unpaired Begin/End and Push/Pop calls. Self-gating: silent on projects without ImGui.
- **Bundled assets** — a drop-in `imscoped.hpp` (RAII guards), CMake / `main.cpp` scaffolding templates, and an `imgui_test_engine`-based test skeleton.

## Install

```text
/plugin marketplace add <github-user>/claude-imgui-cpp
/plugin install imgui-cpp@<github-user>
```

## What it knows

- The five pillars: bootstrapping new ImGui apps, navigating and extending existing ImGui codebases, building custom widgets via `ItemAdd` / `DrawList`, doing styling/layout/font/DPI work, and avoiding the well-known pitfalls (scroll-window sizing, child-frame growth, ID stack collisions, paired-call mismatches).
- Default conventions: RAII scope guards, `std::expected` at API boundaries, `std::print` for diagnostics, no modules in v1, strict ID-stack hygiene.
- LSP-driven navigation of ImGui's monofiles: `workspaceSymbol` for symbol search, `documentSymbol` for monofile TOCs, `goToDefinition` / `findReferences` / `hover` for navigation.
- The v1.92 changelog distilled, so historical workarounds known to be obsolete don't get recommended.

## What it doesn't ship in v1

The following are tracked as Linear feature requests and will be added in subsequent releases:

- **Backends:** Vulkan, DirectX 11, DirectX 12, Metal, WebGPU, SDL3 platform.
- **Build systems:** Meson + WrapDB, Bazel + `http_archive`, Premake5, raw Makefiles. CMake + FetchContent is first-class in v1; other build systems get a generic fallback.

## Repo orientation

| Path | Purpose |
|---|---|
| `skills/imgui-cpp-development/` | The shipped skill (parent `SKILL.md` + references + scripts + assets) |
| `commands/` | Slash commands |
| `hooks/` | Post-edit hooks |
| `.claude-plugin/` | Plugin manifest + marketplace metadata |
| `evals/` | skill-creator eval fixtures (test prompts + assertions) |
| `vendor/` | Upstream sources we research against — `.gitignored`, recreate via `scripts/setup-vendor.sh` |
| `docs/superpowers/` | Design specs and implementation plans |
| `CLAUDE.md` | Developer guidance for working in this repo |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Skill changes go through the `skill-creator` skill; the eval suite verifies trigger accuracy and routing accuracy on every change.

## License

MIT — see [LICENSE](LICENSE). Dear ImGui itself is independently MIT-licensed; this plugin does not bundle upstream source.
