# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

A Claude Code **plugin** that ships an `imgui-cpp-development` skill (plus slash commands and hooks) for working with [Dear ImGui](https://github.com/ocornut/imgui) in C++23. Pinned upstream target: **v1.92.7-docking**. License: MIT.

This file is *for developers of this plugin*. The shipped skill has its own audience-facing documentation in `skills/imgui-cpp-development/SKILL.md`. Don't conflate the two — what's true for *us* (vendor-grounded research, eval-driven changes) is not what the shipped skill should tell its users.

## Architecture

```
.claude-plugin/        plugin manifest + marketplace metadata
skills/                the imgui-cpp-development skill (parent SKILL.md + references/ + scripts/ + assets/)
commands/              slash commands that route through the skill
hooks/                 post-edit hooks (non-blocking, report-only)
docs/superpowers/      design specs and implementation plans for our brainstorming/dev workflow
evals/                 skill-creator eval fixtures (test prompts + assertions)
vendor/                .gitignored — upstream sources we research against (recreate via scripts/setup-vendor.sh)
scripts/               dev-time scripts (setup-vendor.sh, etc.)
```

The shipped skill is structured for **independent loadability**: every file under `skills/imgui-cpp-development/references/` stands alone, so the model loads exactly the docs needed for the task at hand and nothing else. The parent `SKILL.md` is a thin router; sub-docs do not implicitly depend on each other.

## Development workflow

### Bring up `vendor/` first

`vendor/` is the source of truth for everything in `references/`. **Always run** before substantive skill work:

```bash
bash scripts/setup-vendor.sh
```

This pulls Dear ImGui at the pinned `v1.92.7-docking` tag, plus GLFW and `imgui_test_engine` (for backend / testing research). Don't write skill content from training-data memory — read the actual source first. Dear ImGui's monofiles (`imgui.h`, `imgui.cpp`, `imgui_demo.cpp`, `imgui_internal.h`) are themselves the canonical reference, by upstream's own description.

### Use clangd / LSP for navigating ImGui

Generate `compile_commands.json` for `vendor/imgui` once (instructions in `skills/imgui-cpp-development/references/lsp-navigation.md`), then use the `LSP` tool's `workspaceSymbol` for "find this function" lookups, `documentSymbol` for monofile TOCs, and `goToDefinition` / `findReferences` / `hover` for navigation. This is dramatically more reliable than grep on a 30k-line monofile.

### Every skill change goes through skill-creator

When editing `SKILL.md` or any file under `skills/imgui-cpp-development/`, route through the `skill-creator` skill. Reasons:

- Description-string changes affect trigger accuracy. The skill-creator evals verify trigger precision/recall across realistic prompts, including should-not-trigger negatives.
- Reference-doc changes affect routing accuracy and answer quality. The eval loop catches regressions a human reviewer would miss.
- Bundled scripts/assets benefit from the "do all test cases reinvent this?" check that skill-creator's transcript review surfaces.

The eval loop lives in `evals/` (test prompts + assertions). New significant content additions warrant a new eval test case.

### Issue routing

- **Backend support requests** (Vulkan/DX11/DX12/Metal/WebGPU/SDL3/etc.) → Linear feature requests, team `main`. Already filed: MAIN-2…MAIN-7.
- **Build-system support requests** (Meson/Bazel/Premake/Makefile) → Linear feature requests, team `main`. Already filed: MAIN-8…MAIN-11.
- **Newly discovered ImGui pitfalls** → research note in `vendor/notes/issues/<topic>.md` (gitignored), then promote to `references/pitfalls-catalog.md` + the relevant deep-dive doc when validated.
- **Friction with this plugin's tooling itself** (eval flow, vendor setup, hook noise) → Linear `friction` label.

### Never use emoji characters

DO NOT EVER use emojis, unless it's obvious that emojis are SPECIFICALLY called for, as in it is EXPLICITLY asked for from the user (e.g. the user uses emoji characters in his instructions, or the project is OBVIOUSLY using them already for some reason. This will only EVER be applicable to projects where emoji characters need to be supported or a project the user didn't write. **Emojis are weird to see in a terminal, and are unprofessional when used in markdown/README.md, or really any other text.**

This applies to: code, scripts, prompts, READMEs, commit messages, CLAUDE.md edits, memory entries, Linear issues, and anything else with text. Stars (★), checkmarks (✓), warning signs (⚠), gears (⚙), etc. all count as emoji-grade decoration and should not appear.

### Search vague tool errors early; don't bisect for hours

When a Claude Code (or any) error message:

- Tells the user to do something that obviously isn't the cause (e.g., "Update Claude Code" when a structurally-similar plugin works in the same session), OR
- Is generic when the failure looks specific, OR
- Survives 1–2 reasonable local-config bisect attempts

…stop poking and **web-search the exact error text plus the tool name**. Most user-hostile error strings map to known upstream bugs. The cost of an extra search is trivial; the cost of three more bisect rounds isn't.

For Claude Code specifically, prefer searching `github.com/anthropics/claude-code/issues`. We hit `Failed to install: This plugin uses a source type your Claude Code version does not support.` early in this repo's bootstrap and bisected for ~2 hours through manifest fields, hooks layout, directory structure, etc. — the actual cause was a substring collision in Claude Code's marketplace-name validator (sibling of [issue #56043](https://github.com/anthropics/claude-code/issues/56043)). The marketplace was named `claude-imgui-cpp`; renaming to `imgui-cpp-local` resolved it instantly. One search of the error string would have surfaced the matching report immediately.

### Plugin install caveat — marketplace name avoids `claude-` prefix

This repo's marketplace is named `imgui-cpp-local` (not `claude-imgui-cpp`) because Claude Code's marketplace-name validator silently rejects certain substring patterns at install time with a misleading "source type not supported" error. The `claude-` prefix appears to be one of those colliding patterns. If you rename the marketplace, test `/plugin install <plugin>@<new-name>` from a fresh session before assuming the new name works.

### Tool discipline (inherited from the user's global CLAUDE.md, summarized here)

- Prefer `Read` / `Edit` / `Write` over `Bash(cat/sed/echo)` for file work.
- Prefer `LSP` over grep for symbol navigation in the vendored monofiles.
- For broad codebase questions, dispatch the `Explore` subagent rather than chaining greps.
- Don't run pyright proactively; rely on `<new-diagnostics>` arrivals.
- Subagents inherit no CLAUDE.md context — every dispatched subagent prompt must include the tool rules it needs.
- Treat `vendor/` as read-only research. Never edit upstream source — write findings to `vendor/notes/` (gitignored) or directly into a reference doc.

### Worktree-driven development; merges to `main` only on request

For non-trivial work in this repo, follow this loop:

1. **Create a worktree before starting.** Use the `superpowers:using-git-worktrees` skill, or run `git worktree add ../imgui-cpp-plugin-<topic-slug> -b <topic-slug>` directly. The worktree is where all edits land; `main`'s working tree stays clean.
2. **Commit freely inside the worktree.** Small, well-described commits are encouraged — they're free to make and they let the user diff your work checkpoint-by-checkpoint instead of squinting at one giant working-tree diff. No need to ask before each commit; the worktree is your scratch space.
3. **Don't merge to `main` on your own initiative.** The user reviews the worktree's commits and explicitly says "merge", "land", or "ship X" before any worktree commits cross over to `main`. Until then, `main` does not change. When the merge is requested, fast-forward or rebase the worktree branch onto `main` — preserve the commit history rather than squashing unless the user asks.
4. **Trivial edits to `main` are OK only for the smallest in-place fixes** the user explicitly asked for — typos, single-line tweaks during an active conversation about that file. Anything that takes more than a couple of edits should branch off into a worktree first.
5. **Never force-push, never amend `main`'s shared history**, never delete a worktree branch the user hasn't merged or explicitly abandoned. The worktree IS the audit trail; preserving it preserves the user's ability to review.

Why: this gives the user fine-grained review on every change without slowing development inside the worktree, and keeps `main` exactly as the user last approved it. It also matches how the plugin install path consumes the repo — `/plugin install` reads from the filesystem at the marketplace's registered location (`main`'s working tree), so users always get a published-and-approved state until a merge ships new work.

## Default conventions the shipped skill recommends

These are baked into the skill content; mirrored here so dev-time edits stay aligned:

- **RAII scope guards by default** — see `assets/imscoped.hpp`. Pair every Begin/End-style call.
- **`std::expected<T, GfxError>` at API boundaries** for fallible resource ops.
- **`std::print` / `std::println`** for diagnostics.
- **No modules, no coroutines, no aggressive ranges-based widget views in v1.**
- **Strict ID-stack hygiene** — `PushID(ptr)` for objects, `PushID(int)` for stable indices, never bare auto-labels in loops.
- **Begin/End pairing rules** — top-level `Begin` always paired with `End` regardless of return; `BeginChild` / `BeginPopup` / `BeginTreeNode` pair `End*` only when the call returned true. Encode this at compile time via the scope guards.

If a dev-time edit changes one of these, update both this file and `skills/imgui-cpp-development/SKILL.md` in the same change.

## Common commands

```bash
# Bring up vendor sources (run after a fresh clone)
bash scripts/setup-vendor.sh

# Run the skill eval loop (against current SKILL.md + references)
bash evals/run-evals.sh

# Locate ImGui in a target project (sanity-check the locate-imgui flow)
bash skills/imgui-cpp-development/scripts/locate-imgui.sh /path/to/some/cpp/project
```
