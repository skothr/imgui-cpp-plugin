# imgui-cpp Plugin & Skill — Design Spec

- **Status:** draft, pending user review
- **Date:** 2026-05-07
- **Owner:** skothr@gmail.com
- **Target version of upstream:** Dear ImGui `v1.92.7-docking`
- **Target C++ standard:** C++23
- **Distribution:** Claude Code plugin, GitHub-hosted, MIT-licensed

## Summary

Build a Claude Code plugin (`claude-imgui-cpp`) that ships a single, hot-swappable skill for working with Dear ImGui in modern C++ — bootstrapping new apps, navigating and extending existing ImGui codebases, authoring custom widgets, doing styling/layout/font/DPI work, and avoiding the ImGui pitfalls that are well-known to users of the library but absent from any single canonical document. The skill leans on Dear ImGui's source-as-documentation by directing the model to use clangd / LSP + grep on whichever ImGui copy lives in the user's project, rather than bundling source. Hard documentation (the upstream repo at the pinned tag, GLFW, imgui_test_engine, scraped issue threads) is pulled into a top-level `vendor/` directory that is `.gitignored` and used only during plugin development.

Production goal: a v1.0 release on GitHub installable via `/plugin marketplace add` + `/plugin install`, covering OpenGL3 + GLFW first-class, with all other backends and build systems filed as Linear feature requests for phased delivery.

## Non-goals

- Bundling Dear ImGui source with the plugin (license-compatible, but creates version-drift risk and bloats the plugin).
- Building a custom C++ symbol-lookup tool. clangd via the existing `LSP` tool covers it: `workspaceSymbol` for symbol search, `documentSymbol` for monofile TOC, `goToDefinition` / `findReferences` / `hover` for navigation.
- Auto-modifying user code via hooks. Hooks are report-only.
- Supporting build systems or backends beyond the v1 scope; those are tracked as Linear issues, not stub-implemented in v1.
- C++23 modules, coroutines, or aggressive ranges-based widget views. The skill recommends RAII guards, `std::expected` at API boundaries, `std::print` for diagnostics, and otherwise stays close to ImGui's C-style surface.

## Scope (the five pillars)

The skill optimizes for these tasks, in priority order. Each maps to one or more sub-docs in `references/`.

1. **Bootstrap new ImGui apps** — pick a backend, set up a build, write a minimal main loop, render the demo window.
2. **Work in existing ImGui codebases** — navigate, extend, refactor; integrate with host event loops/renderers.
3. **Build custom widgets / draw-list work** — `ItemAdd` / `ItemSize` / `ButtonBehavior`, `DrawList` primitives, hit-testing, keyboard nav.
4. **Style, layout, theming, fonts** — style stack discipline, font atlas / DPI / multi-viewport correctness, docking layouts, ini persistence.
5. **Common pitfalls and anti-patterns (cross-cutting, first-class)** — known scroll-window sizing bugs, child-frame growth, ID-stack collisions, paired-call mismatches, etc. Includes distilled changelog through v1.92.7-docking so the model knows which historical workarounds are now obsolete.

## Default conventions the skill recommends

- **RAII scope guards by default.** Provide `ImScoped::Window`, `ImScoped::Child`, `ImScoped::ID`, `ImScoped::StyleVar`, `ImScoped::StyleColor`, `ImScoped::Group`, `ImScoped::TreeNode`, `ImScoped::Indent`, etc. Each pairs `Begin`/`End` (or `Push`/`Pop`) via destructor with the documented "End is always called regardless of Begin's return" rule encoded.
- **`std::expected<T, GfxError>` at API boundaries** for fallible resource ops (texture load, shader compile, swapchain init).
- **`std::print` / `std::println`** for diagnostics, not `printf`.
- **No modules, no coroutines, no aggressive ranges-based widget views in v1.** They fight the upstream API surface and add maintenance load without commensurate value.
- **Strict ID-stack hygiene.** `PushID(ptr)` for objects, `PushID(int)` for stable indices, never use auto-generated labels alone in loops.

## Architecture

### Repo layout (this repo, public on release)

```
claude-imgui-cpp/
├── .claude-plugin/
│   ├── plugin.json
│   └── marketplace.json
├── skills/
│   └── imgui-cpp-development/
│       ├── SKILL.md
│       ├── references/
│       │   ├── locate-imgui.md
│       │   ├── bootstrap.md
│       │   ├── frame-loop.md
│       │   ├── id-stack.md
│       │   ├── layout-and-sizing.md
│       │   ├── styling-fonts-dpi.md
│       │   ├── docking-and-viewports.md
│       │   ├── custom-widgets.md
│       │   ├── pitfalls-catalog.md
│       │   ├── changelog-1.92.x.md
│       │   ├── raii-scope-guards.md
│       │   ├── testing.md
│       │   ├── lsp-navigation.md
│       │   └── backends/
│       │       └── opengl3-glfw.md
│       └── scripts/
│           ├── locate-imgui.sh
│           ├── check-imgui-version.sh
│           └── ensure-compile-commands.sh
├── commands/
│   ├── imgui-bootstrap.md
│   ├── imgui-locate.md
│   ├── imgui-review.md
│   └── imgui-pin.md
├── hooks/
│   ├── hooks.json
│   ├── imgui-lint.sh
│   └── imgui-pair.sh
├── docs/
│   └── superpowers/specs/             ← this spec lives here
├── evals/                              ← skill-creator eval fixtures
├── vendor/                             ← .gitignored, dev-only
├── scripts/
│   └── setup-vendor.sh                 ← clone vendor repos at pinned tags
├── CLAUDE.md                           ← dev-time guidance for THIS repo
├── README.md                           ← user-facing
├── CONTRIBUTING.md
├── LICENSE                             ← MIT, matching upstream ImGui
└── .gitignore                          ← excludes vendor/
```

### Two CLAUDE.md audiences (kept separate)

- `/CLAUDE.md` (repo root) — *for us developing this plugin*. Says: route every skill edit through `skill-creator`; treat `vendor/imgui/` as ground truth; run evals before committing skill changes; how to add a new backend doc; etc.
- `skills/imgui-cpp-development/SKILL.md` — *for users of the installed plugin*. Says: the skill description (trigger string), default conventions, the routing decision tree.

### Skill content structure (independence is mandatory)

Every file under `references/` is **independently loadable**. The parent `SKILL.md` is a thin router; it does not duplicate sub-doc content. Each reference file:

- Begins with a one-line scope statement: "Load this file when …"
- Lists at most one or two siblings it suggests loading alongside (only when truly necessary; default is "load just this one").
- Contains a self-contained code skeleton or recipe wherever code is shown.

Backends are the canonical example: any given user project uses one backend, so the skill loads exactly one `backends/<name>.md` and never the others. Each backend doc covers init, per-frame loop, image/texture ID semantics, shutdown, version-compatibility notes, and pitfalls — all in one self-contained file.

The parent `SKILL.md` carries:

- Frontmatter `name` + `description` (the trigger string; tuned via skill-creator evals — initial draft below).
- A two-step decision flow: (1) run locate-imgui first to establish what version we're working against; (2) load the smallest set of sub-docs needed for the task.
- Default conventions block (the bullet list above), terse.
- A pointer index: `task → which sub-doc to load`.

Initial trigger string draft (will be eval-tuned):

> Use when working with Dear ImGui in C++ (v1.92.x docking branch). Covers bootstrapping new apps, extending existing ImGui codebases, custom widgets via DrawList/ItemAdd, style/layout/font/DPI work, and known pitfalls (scroll-window sizing, child-frame growth, ID stack quirks, etc.). Recommends RAII scope guards and C++23 idioms.

### Slash commands (v1)

Each command is a thin `commands/*.md` file whose body enters through the skill so conventions are always applied — commands do not duplicate skill knowledge.

| Command | Routes to | Purpose |
|---|---|---|
| `/imgui-bootstrap` | `bootstrap.md` + matching backend doc | scaffold a new ImGui project (asks build system + backend) |
| `/imgui-locate` | `locate-imgui.md` + script | scan cwd for ImGui copies, report version/path |
| `/imgui-review` | `pitfalls-catalog.md` + relevant area docs | audit current file or branch for known pitfalls |
| `/imgui-pin` | `locate-imgui.md` | pin a specific ImGui version + record it in repo |

### Hooks (v1)

Two hooks, both `PostToolUse` on `Edit|Write` for `*.cpp`/`*.h`/`*.hpp`/`*.cxx`. **Both are non-blocking and report-only** (always exit 0; warnings on stderr).

| Hook | What it does |
|---|---|
| `imgui-lint.sh` | Fast regex scan for a small, high-precision rule set: missing `PopID` for a `PushID`, suspicious `Begin` without matching `End`, `SetNextWindowSize` after `Begin`, common `BeginChild` padding-growth pattern. Per-rule opt-out: `// imgui-lint: allow <rule>` |
| `imgui-pair.sh` | Cross-file scan for unpaired Begin/End and Push/Pop pairs in the edited file |

**Self-gating** — both hooks exit 0 silently when:

- `IMGUI_LINT_DISABLE=1` is set, OR
- No `imgui.h` is reachable from cwd via the locate-imgui search roots.

This matters because the plugin is install-once-globally; the user shouldn't see imgui complaints while editing unrelated projects.

**Hooks deliberately do NOT:**

- Invoke clang-format (user's choice, not ours).
- Invoke a compiler / build (too slow, too project-specific).
- Block edits (`exit 0` always).

## Doc strategy

### `vendor/` (top-level, `.gitignored`) — research scratch

Populated by `scripts/setup-vendor.sh`, which we (developers/contributors) run once after cloning. Contents:

```
vendor/
├── imgui/                  upstream repo at tag v1.92.7-docking
├── glfw/                   upstream GLFW for backend research
├── imgui_test_engine/      upstream test engine for testing sub-doc research
└── notes/
    ├── issues/             curated GitHub issue scrapes (per-pitfall provenance)
    └── faq.md              upstream FAQ snapshot
```

`vendor/` is invisible to the shipped plugin. It is how we *write* the references; it is not what users get. Setup script pins exact tags/commits so research is reproducible.

### Locate-imgui procedure (the skill's first action)

`references/locate-imgui.md` + `scripts/locate-imgui.sh`:

1. **Scan** the user's project for `imgui.h` in common roots: `./`, `third_party/`, `external/`, `vendor/`, `deps/`, `lib/`, `src/`, CMake `build/_deps/` (FetchContent), and Conan / vcpkg cache locations (when discoverable).
2. **Identify version**: parse `IMGUI_VERSION_NUM` from `imgui.h`; detect docking branch via the `IMGUI_HAS_DOCK` macro.
3. **If not found**, present three concrete next steps with prefilled commands and ask the user which path to take:
   - CMake `FetchContent_Declare(... GIT_TAG v1.92.7-docking ...)` snippet
   - `git submodule add` under `third_party/imgui/`
   - Manual download from the upstream repo at the pinned tag
4. **If found but stale** relative to the skill's pinned target, warn and ask before recommending an upgrade. Never auto-upgrade.
5. **Verify `compile_commands.json`** so clangd-driven LSP navigation works. If absent, offer the project-appropriate snippet to enable it (CMake: `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)`; Meson / Bazel / Premake variants documented in their respective build-system docs once added).

This runs implicitly on first ImGui-related action in a session, and explicitly via `/imgui-locate`.

### LSP navigation as the canonical "find ImGui code" answer

`references/lsp-navigation.md` codifies clangd recipes for ImGui's monofile structure:

- **Find a function or symbol:** `LSP workspaceSymbol "ImGui::Begin"` returns the definition site directly.
- **Get a TOC for `imgui.h` / `imgui.cpp`:** `LSP documentSymbol` on the file.
- **Trace usage:** `LSP findReferences` / `goToDefinition` / `hover` once parked on a token.
- **Prerequisite:** `compile_commands.json` is present and includes the ImGui translation units. The locate-imgui procedure verifies this.

The skill never needs a custom symbol-lookup tool; LSP via clangd is the right path.

## Build-system handling

Build-system agnostic by design, but **CMake is first-class in v1**. Other build systems are tracked as Linear feature requests and deepened in phase 2.

- **CMake + FetchContent** — first-class. `/imgui-bootstrap` defaults here. `bootstrap.md` and `locate-imgui.md` cover CMake scenarios in depth.
- **Meson + WrapDB** — Linear feature request.
- **Bazel + `http_archive`** — Linear feature request.
- **Premake5** — Linear feature request.
- **Raw Makefile** — Linear feature request.

When `/imgui-bootstrap` runs, it asks the user which build system; if anything other than CMake is selected, the skill says so explicitly and falls back to a minimal generic flow plus a note that deeper guidance is on the roadmap.

## Backends

OpenGL3 + GLFW is the only first-class backend in v1. All other backends are Linear feature requests (Vulkan, DX11, DX12, Metal, WebGPU, SDL2/3 platform). Each backend is its own self-contained `references/backends/<name>.md` so loading exactly one is sufficient — no implicit cross-references between backends.

## Testing

### Skill itself

`evals/` contains skill-creator-style eval fixtures:

- **Trigger accuracy:** prompts that should fire the skill (varied phrasings of imgui tasks across the five pillars) and prompts that must *not* fire it (general C++, other UI frameworks, unrelated graphics work).
- **Routing accuracy:** given the skill is active, does the model load the right sub-doc(s) for the task?
- **Golden answers:** ~6–8 representative pitfall scenarios with expected fix patterns.
- **Backend specificity:** asking about backend X must not load backend Y's doc.

Evals run before any skill change is committed. `CLAUDE.md` enforces this.

### ImGui apps users build with the skill

`references/testing.md` documents `imgui_test_engine` integration: how to wire it, the headless test pattern, common assertion idioms, and the limits (test_engine doesn't cover render correctness, only logic / interaction). This is reference material, not a runtime dependency.

## Release plan

### v1.0 ships

- Single skill `imgui-cpp-development` with all sub-docs in the architecture diagram **except** backends other than `opengl3-glfw`.
- 4 slash commands (`bootstrap`, `locate`, `review`, `pin`).
- 2 hooks (`imgui-lint`, `imgui-pair`).
- Eval suite covering trigger accuracy, routing, and golden answers.
- `README.md` (install, quick demo, supported scope, known limitations).
- `CONTRIBUTING.md` (vendor setup, eval workflow, skill-creator iteration loop).
- MIT license, tagged `v1.0.0`.

### Distribution

GitHub repo with `.claude-plugin/marketplace.json`. Users install via:

```
/plugin marketplace add <github-user>/claude-imgui-cpp
/plugin install imgui-cpp@<github-user>
```

Optional later: PR to a community marketplace if/when one is canonical.

### Post-v1 phases

- **Phase 2:** Tackle deferred backends and build systems based on Linear priority and user feedback. Each follows the same skill-creator iteration loop (write doc → eval → ship).
- **Phase 3 (only if friction shows up):** Deeper `imgui_test_engine` integration; possibly an `imgui-reviewer` subagent for parallel pitfall sweeps on large codebases.

### Linear feature requests (filed at end of brainstorm)

- Backend: Vulkan + GLFW/SDL
- Backend: DirectX 11 + Win32
- Backend: DirectX 12 + Win32
- Backend: Metal + macOS
- Backend: WebGPU
- Backend: SDL3 platform
- Build system: Meson + WrapDB
- Build system: Bazel + http_archive
- Build system: Premake5
- Build system: raw Makefile

## Open questions (deferred, not blocking v1)

- Final plugin name (`imgui-cpp` vs `claude-imgui-cpp` vs `dear-imgui-cpp`) — bikeshed at release time.
- Whether to publish a per-rule lint config file users can override, or stay with `// imgui-lint: allow <rule>` inline opt-outs only. Decide after eval feedback on false-positive rate.
- Marketplace path beyond GitHub. Defer until a canonical community marketplace exists.

## Risks

- **Trigger-string brittleness.** The skill must fire on natural ImGui phrasing without firing on general C++ work. Mitigation: extensive eval suite, iterate via skill-creator.
- **Hook false positives.** Even at "report only," noisy warnings are annoying. Mitigation: small initial rule set, all rules opt-outable, false-positive rate is an eval metric.
- **Version drift.** Pinning to `v1.92.7-docking` means reference docs go stale as upstream advances. Mitigation: changelog tracking is itself a sub-doc; skill explicitly states the pinned target so users can decide.
- **License posture for `vendor/notes/issues/`.** GitHub issue text isn't blanket redistributable. Mitigation: keep scrapes local, commit only URLs and our own distilled summaries.
