# Contributing to imgui-cpp-plugin

Welcome! This plugin's value comes from being **grounded in actual upstream Dear ImGui source**, not in training-data memory. The contribution flow reflects that.

## One-time setup

```bash
git clone https://github.com/skothr/imgui-cpp-plugin.git
cd imgui-cpp-plugin
bash scripts/setup-vendor.sh
```

The setup script clones Dear ImGui at the pinned `v1.92.7-docking` tag, GLFW 3.4, and `imgui_test_engine` v1.92.7 into `vendor/`. That directory is `.gitignored` — it's research scratch, not redistributable. If `setup-vendor.sh` fails (network, GitHub access), fix the underlying issue rather than working around it; reference docs lose accuracy when contributors substitute training memory for actual source.

## Development workflow

### Use `skill-creator` for every skill change

Editing `skills/imgui-cpp-development/SKILL.md` or any file under `references/` / `assets/` / `scripts/` / `hooks/` should go through the `skill-creator` skill. Reasons:

- The trigger description in `SKILL.md` is what determines whether the skill activates. `skill-creator` runs eval queries (positive + negative) to measure trigger precision/recall and proposes tuned descriptions.
- Reference-doc edits affect routing accuracy. The eval suite catches regressions a human reviewer would miss.

If you change something substantive, expand the eval suite at the same time (add a new entry to `evals/evals.json`) so future contributors don't accidentally break the behavior.

### Ground every claim in `vendor/`

When writing or editing a reference doc:

- **Read the actual upstream file** for any code claim, function signature, or behavior you cite.
- **Cite as `path:line`** so reviewers can verify quickly. The convention is `imgui.h:NNN` / `imgui.cpp:NNN` / `imgui_demo.cpp:NNN` / `docs/FAQ.md:NNN` / `docs/CHANGELOG.txt:NNN`.
- For non-trivial sections, prefer **navigating via clangd / LSP** over grep — `compile_commands.json` for `vendor/imgui/` makes this fast. See `skills/imgui-cpp-development/references/lsp-navigation.md`.

If you can't find the upstream evidence for a claim, drop the claim. Speculative guidance is a net-negative.

### House style for reference docs

- Open with `> **Load this file when:** <one-line scope>` block quote. The model uses this to decide whether to load the doc.
- Imperative voice. Explain WHY before HOW.
- Avoid all-caps directives (NEVER, ALWAYS, MUST). Reasoning beats decree.
- Code examples use `ImScoped::*` from `assets/imscoped.hpp` for any Begin/End or Push/Pop pair.
- Each doc is **independently loadable** — no implicit dependencies on a sibling doc. Cross-link via relative paths in a "See also" block at the end (1–3 siblings max).
- Length target: 1500–2500 words depending on topic complexity. If you're approaching 3000 words, split into two docs.

### Run evals before committing skill changes

```bash
# Inside Claude Code, with the skill installed:
# 1. Use the skill-creator skill (Skill tool) and describe the change you made.
# 2. It will spawn parallel with-skill / without-skill subagents per eval prompt.
# 3. Review the eval-viewer's outputs for regressions.
# 4. If the eval surfaces a problem, fix it before committing.
```

For description-string changes specifically, run the trigger-accuracy optimizer:

```bash
python -m scripts.run_loop \
  --eval-set evals/trigger-eval.json \
  --skill-path skills/imgui-cpp-development \
  --max-iterations 5
```

(Run this inside the skill-creator's working environment; the script lives in the `superpowers` plugin.)

### Adding a backend doc

Backends other than OpenGL3+GLFW are tracked as Linear feature requests (MAIN-2 through MAIN-7). To add one:

1. Pick the corresponding Linear issue and assign it.
2. Read `vendor/imgui/backends/imgui_impl_<backend>.{h,cpp}` cover-to-cover.
3. Read `vendor/imgui/examples/example_<backend>/main.cpp` cover-to-cover.
4. Write the new doc at `skills/imgui-cpp-development/references/backends/<backend>.md` following the template established by `opengl3-glfw.md`. The doc must be self-contained — load only it for projects targeting that backend.
5. Add a row to `SKILL.md`'s routing table.
6. Add at least one eval prompt that exercises the new backend (e.g., "I'm using Vulkan with ImGui, how do I…").
7. Update `README.md`'s "What it doesn't ship in v1" section to remove the backend from the deferred list.

### Adding a build-system flow

Build systems other than CMake are also tracked as Linear feature requests (MAIN-8 through MAIN-11). The flow is similar:

1. Update `bootstrap.md` and `locate-imgui.md` with build-system-specific scaffolding.
2. Update `scripts/locate-imgui.sh` to recognize that build system's typical layout.
3. Update `scripts/ensure-compile-commands.sh` with the right snippet for the build system.
4. Add an eval prompt that exercises a build-system-specific question.

## Issue routing

- **New backends, build systems, asset templates** → Linear feature request, team `main`.
- **Newly discovered ImGui pitfalls** → research note in `vendor/notes/issues/<topic>.md` (gitignored), then promote to `references/pitfalls-catalog.md` plus the relevant deep-dive doc when you've confirmed the reproducer.
- **Friction with this plugin's own tooling** (eval flow, vendor setup, hook noise) → Linear `friction` label.
- **Plugin name / branding** → leave for the maintainer; bikeshed at release.

## Code of conduct

Be respectful. The Dear ImGui community has a strong "show me a reproducer" culture; we follow it here too. When opening a PR, attach:

- A specific upstream `path:line` citation for any factual claim added.
- Eval results showing no regressions on existing prompts.
- A new eval prompt if you added meaningful new content.

## License

By contributing, you agree your contributions are licensed under the MIT License (see `LICENSE`).
