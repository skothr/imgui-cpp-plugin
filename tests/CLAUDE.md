# CLAUDE.md — test sessions for the `imgui-cpp` plugin

This directory exists to manually exercise the installed `imgui-cpp` Claude Code plugin in fresh sessions, so we can observe end-to-end skill behavior on realistic prompts and refine it from real evidence.

This file is read by Claude when a test session starts inside `tests/`. It is intentionally separate from the repo-root `CLAUDE.md` (which is for plugin developers) — this one's audience is "Claude running the test."

## You are in a TEST SESSION for the imgui-cpp plugin

- The `imgui-cpp:imgui-cpp-development` skill is expected to be loaded and active. Other `imgui-cpp:*` slash commands should also be visible: `imgui-bootstrap`, `imgui-locate`, `imgui-review`, `imgui-pin`. If any of those are missing from the available-skills list at session start, **STOP and tell the user** — that's a triggering / install regression and the test result isn't valid until it's fixed.
- The user's first real message in a test session is usually the verbatim contents of one of the files under `prompts/`. Don't second-guess it; treat it as the actual user message.
- Each prompt names an **output subdirectory** under `tests/<NN>-<slug>/`. Create it and put every artifact there. **DO NOT scribble in the parent project tree** (`skills/`, `commands/`, `.claude-plugin/`, `assets/`, `references/`, etc.) — those are the installed plugin's source and must not be mutated by a test run. The test's job is to demonstrate the skill, not to edit it.

## Per-prompt expectations

- **Greenfield-scaffold prompts** (build a new app from zero): produce a working project layout under the output subdir — `CMakeLists.txt`, `src/main.cpp`, `src/imscoped.hpp`, plus a short `README.md` with the build/run command. Use the **bundled assets** as your starting point (the skill's `assets/CMakeLists-glfw-opengl3.txt.template` and `main_glfw_opengl3.cpp.template` are there to be copied, not retyped). Do not actually invoke `cmake` or `make` inside the test session unless the user asks — the build needs network and dev tooling we likely don't have. Producing the correct files is the test.
- **Diagnostic prompts** (broken code, fix it): produce a single `response.md` in the output subdir containing diagnosis, root cause cited from the upstream source / pitfalls catalog, the corrected code, and a "see also" pointer to the relevant `references/<topic>.md` the skill routed you to. No project scaffold needed.
- **Mixed prompts**: do whatever the prompt body actually asks for; the categories above are guidelines, not a strict schema.

## What we're observing

When you run a prompt, we'll later inspect both the `tests/<NN>-<slug>/` artifacts and the session transcript for:

- **Trigger accuracy** — did `imgui-cpp:imgui-cpp-development` activate on its own from the prompt phrasing, or did you have to invoke it manually?
- **Routing accuracy** — did you load the right reference doc(s) and avoid loading siblings the prompt doesn't need?
- **Idiom adoption** — RAII scope guards from `imscoped.hpp`, `std::print` for diagnostics, the v1.92 font defaults, the docking-branch IO config flags, etc.
- **Diagnosis correctness** (for debug prompts) — did you hit the canonical root cause that `references/pitfalls-catalog.md` documents?
- **Output portability** — does the produced project actually scaffold cleanly without the user having to fix paths or re-include vendored content?

If you notice the skill misbehaving (mis-trigger, wrong routing, stale advice, false-positive lint), file a friction-labeled Linear issue or note it explicitly in your reply so the user can capture it.

## Adding a new prompt (for the user / a future me)

1. Pick the next unused number under `prompts/`.
2. Name the file `<NN>-<short-slug>.md`. Body is a self-contained user message — first-person, realistic in tone, no meta-commentary about it being a test.
3. Specify the output subdirectory `tests/<NN>-<short-slug>/` inside the prompt body so a test session knows where to write.
4. For debug prompts, include the broken code inline as a fenced code block.
5. Add a one-line description to `tests/README.md`'s prompt table so the index stays current.

## Test outputs are not gitignored by default

Test sessions create real subdirectories with real files under `tests/`. They are not in `.gitignore` — commit them when a run produces useful evidence (for example, a botched diagnosis worth referencing in a Linear issue, or a polished scaffold worth using as a baseline). If you produce throwaway artifacts you don't want in history, delete the subdir before exiting the session.
