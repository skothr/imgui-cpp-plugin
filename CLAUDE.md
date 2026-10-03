# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

A Claude Code **plugin** that ships an `imgui-cpp-development` skill and slash commands for working with [Dear ImGui](https://github.com/ocornut/imgui) in C++23. Pinned upstream target: **v1.92.7-docking**. License: MIT.

This file is for developers of the plugin. The shipped skill has its own audience-facing documentation in `skills/imgui-cpp-development/SKILL.md`. What is true for plugin developers (vendor-grounded research, eval-driven changes) is not what the shipped skill should tell its users.

**This repo is public.** Nothing committed or posted here (files, commit messages, PR and issue text) may contain absolute local paths, machine-specific or personal configuration, credentials, or identifiers from private trackers. Use repo-relative paths and placeholders such as `<repo-root>`. Known leftover: two comment lines in `tests/run-prompt.py` still cite ids from the tracker used before GitHub Issues.

## Architecture

```
.claude-plugin/   plugin manifest + marketplace metadata
skills/           the imgui-cpp-development skill (SKILL.md + references/ + scripts/ + assets/)
commands/         slash commands that route through the skill
evals/            skill-creator fixtures: evals.json (answer quality), trigger-eval.json (trigger accuracy)
tests/            manual end-to-end prompt harness (run-prompt.py, prompts/); has its own CLAUDE.md for test sessions
scripts/          dev-time scripts (setup-vendor.sh)
vendor/           gitignored: upstream sources to research against (recreate via scripts/setup-vendor.sh)
docs/superpowers/ gitignored: local design specs, not published
```

The shipped skill is structured for **independent loadability**: every file under `skills/imgui-cpp-development/references/` stands alone, so the model loads only the docs the task needs. The parent `SKILL.md` is a thin router; sub-docs do not depend on each other.

The plugin ships no hooks. The paired-call and pitfall lints are scripts (`imgui-pair.sh` and `imgui-lint.sh` under `skills/imgui-cpp-development/scripts/`) that `/imgui-review` runs.

## Development workflow

### Bring up `vendor/` first

`vendor/` is the source of truth for everything in `references/`. Run this before substantive skill work:

```bash
bash scripts/setup-vendor.sh
```

It pulls Dear ImGui at the pinned `v1.92.7-docking` tag, plus GLFW and `imgui_test_engine`. Do not write skill content from training-data memory; read the source first. Dear ImGui's monofiles (`imgui.h`, `imgui.cpp`, `imgui_demo.cpp`, `imgui_internal.h`) are the canonical reference, by upstream's own description.

Treat `vendor/` as read-only. Never edit upstream source; write findings to `vendor/notes/` (gitignored) or into a reference doc.

### Use clangd / LSP for navigating ImGui

Generate `compile_commands.json` for `vendor/imgui` once (instructions in `skills/imgui-cpp-development/references/lsp-navigation.md`). Then use the `LSP` tool: `workspaceSymbol` to find a function, `documentSymbol` for a monofile's table of contents, and `goToDefinition` / `findReferences` / `hover` for navigation. Grep on a 30k-line monofile returns too many textual matches to be reliable for symbol lookups.

### Every skill change goes through skill-creator

When editing `SKILL.md` or any file under `skills/imgui-cpp-development/`, route through the `skill-creator` skill:

- A description-string change affects trigger accuracy. `evals/trigger-eval.json` measures precision and recall across realistic prompts, including should-not-trigger negatives.
- A reference-doc change affects routing accuracy and answer quality. `evals/evals.json` holds the cases that catch regressions.
- Bundled scripts and assets benefit from skill-creator's transcript review, which shows when every test case reinvents the same helper.

There is no standalone eval runner script (#11); skill-creator drives the fixtures in `evals/`. A significant content addition gets a new eval case.

For an end-to-end check of the plugin in a fresh session, use `tests/run-prompt.py` (usage in `tests/README.md`).

### Issue routing

Everything is tracked in this repo's GitHub Issues. Search before filing.

- **Backend support** (Vulkan, DX11, DX12, Metal, WebGPU, SDL3): existing issues #27-#32.
- **Build-system support** (Meson, Bazel, Makefile, Premake): existing issues #24-#26; Premake has none yet.
- **Newly discovered ImGui pitfalls**: research note in `vendor/notes/issues/<topic>.md` (gitignored), then promote to `references/pitfalls-catalog.md` and the relevant deep-dive doc once validated.
- **Friction with this plugin's own tooling** (eval flow, vendor setup, test harness): issue with the `friction` label.

### No emoji or decorative symbols in repo content

Committed files, commit messages, and PR and issue text contain no emoji and no decorative symbols (stars, checkmarks, warning signs, gears). The only exception is content that must exercise such characters, such as a font-glyph example.

### Branches and PRs

- Work on a feature branch off an up-to-date `main`. Nothing is committed directly to `main`.
- Commit in small, well-described steps. Push the branch and open a PR; a human merges it.
- Never auto-merge, never force-push or amend `main`, and never delete an unmerged branch or its worktree unless the owner has abandoned it.
- When a merge is requested, preserve the commit history rather than squashing unless asked.

`main` must always be a shippable state: a marketplace added from GitHub installs the default branch, so a merge is a release to users. A marketplace added by local path installs whatever is checked out in that directory, so while a feature branch is checked out in the main checkout, a local-path install serves that branch.

### Plugin install caveat: marketplace name avoids the `claude-` prefix

The marketplace is named `imgui-cpp-local`, not `claude-imgui-cpp`. Claude Code's marketplace-name validator rejects certain substring patterns at install time with a misleading error: `Failed to install: This plugin uses a source type your Claude Code version does not support.` The `claude-` prefix appears to be one such pattern (related: [claude-code issue #56043](https://github.com/anthropics/claude-code/issues/56043)). After renaming the marketplace, test `/plugin install <plugin>@<new-name>` from a fresh session.

That error cost about two hours of bisecting manifest fields before a search of the exact error text found the cause. When a tool's error message is generic or points at something that is not the cause, search the exact text (for Claude Code, in `github.com/anthropics/claude-code/issues`) before bisecting local config.

## Default conventions the shipped skill recommends

These are defined in `skills/imgui-cpp-development/SKILL.md` ("Default conventions") and mirrored here so dev-time edits stay aligned:

1. **RAII scope guards for every paired call**: see `assets/imscoped.hpp`.
2. **Begin/End pairing rules**: `Begin` and `BeginChild` pair with their `End` regardless of the return value; every other `Begin*` that returns `bool` pairs with `End*` only when it returned true. The scope guards encode this.
3. **`std::expected<T, GfxError>` at API boundaries** for fallible resource ops.
4. **Diagnostics default to `std::fprintf` / `std::printf`**; `std::print` / `std::println` only where the toolchain is confirmed to support `<print>`.
5. **Strict ID-stack hygiene**: `PushID(ptr)` for objects, `PushID(int)` for stable indices, no bare auto-labels in loops.
6. **No modules, no coroutines, no aggressive ranges-based widget views in v1.**
7. **Third-party headers are SYSTEM includes** in generated CMake.
8. **Cite upstream line numbers only from references loaded in the current session.**

If a dev-time edit changes one of these, update this file and `SKILL.md` in the same change.

## Common commands

```bash
# Bring up vendor sources (run after a fresh clone)
bash scripts/setup-vendor.sh

# Run one end-to-end test prompt against the plugin (from tests/)
./run-prompt.py <NN>-<slug>

# Locate ImGui in a target project (sanity-check the locate-imgui flow)
bash skills/imgui-cpp-development/scripts/locate-imgui.sh /path/to/some/cpp/project
```
