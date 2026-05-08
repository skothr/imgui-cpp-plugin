# tests/

Manual test harness for the `imgui-cpp` Claude Code plugin. Each file under `prompts/` is a self-contained user message you can paste as the first turn of a fresh Claude Code session opened inside this directory. Each prompt directs the session to produce its output under `tests/<NN>-<slug>/`.

See [CLAUDE.md](CLAUDE.md) for the test-session contract (what counts as a passing test, what the session is allowed to touch, what to flag when the skill misbehaves).

## Prompts

| ID | Prompt | Type | What it tests |
|---|---|---|---|
| 01 | [hello-world-glfw](prompts/01-hello-world-glfw.md) | bootstrap | greenfield CMake + GLFW + OpenGL3 scaffold; demo window renders |
| 02 | [tools-panel](prompts/02-tools-panel.md) | bootstrap | basic widget submission (sliders, checkbox, color edit); `std::print` adoption |
| 03 | [sortable-file-table](prompts/03-sortable-file-table.md) | bootstrap | `BeginTable` + sorting + row spanning; `ImGuiListClipper` mention |
| 04 | [debug-delete-button](prompts/04-debug-delete-button.md) | diagnostic | ID-stack collision routing → `references/id-stack.md` |
| 05 | [debug-child-grows](prompts/05-debug-child-grows.md) | diagnostic | Layout/sizing feedback-loop routing → `references/layout-and-sizing.md` (★ user-flagged pain area) |
| 06 | [debug-popup-closes](prompts/06-debug-popup-closes.md) | diagnostic | OpenPopup-every-frame routing → `references/modals-and-popups.md` |
| 07 | [docking-editor-layout](prompts/07-docking-editor-layout.md) | bootstrap (advanced) | DockBuilder programmatic layout + multi-viewport |
| 08 | [knob-widget](prompts/08-knob-widget.md) | custom-widget | `ItemAdd` / `ButtonBehavior` / `DrawList` routing → `references/custom-widgets.md` |

## Running a test (the user's workflow)

1. `cd <repo-root>/tests`
2. Open a fresh Claude Code session there (so the session reads `tests/CLAUDE.md` as project instructions instead of the repo-root `CLAUDE.md`).
3. Paste the contents of `prompts/<NN>-<slug>.md` as the first user message.
4. Watch what happens: did the skill auto-trigger? Which sub-doc did it load? Did it use `imscoped.hpp`?
5. Inspect the produced subdir under `tests/<NN>-<slug>/`.
6. Compare the output against the corresponding routing/idiom expectations and note any misbehavior.

## What success looks like

- **Bootstrap prompts**: session produces a project that *would* compile (CMake fetches ImGui v1.92.7-docking + GLFW 3.4, `main.cpp` opens a GLFW window with the requested ImGui surface, RAII guards in use).
- **Diagnostic prompts**: session diagnoses the canonical root cause documented in `references/pitfalls-catalog.md` and points the user at the right deep-dive reference. Bonus points for citing upstream source as `path:line`.

## Feedback loop

After each test run:

- If the skill behaved correctly: note what worked. Repeat-runs build confidence.
- If the skill misbehaved: capture the failure mode in a Linear issue (label `friction` for skill UX issues, `bug` for actually-wrong content). The skill-creator iteration loop in `evals/` is the right place to formalize a regression test for it.
- Persistent test outputs that document interesting failure modes are worth committing; throwaway successful runs aren't.

## Adding more prompts

Bump the number, name it `<NN>-<slug>.md` under `prompts/`, follow the existing prompts' first-person tone (no "you are testing X" meta-language — it should read like a real user message), and add a row to the table above.
