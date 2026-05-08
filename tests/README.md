# tests/

Manual test harness for the `imgui-cpp` Claude Code plugin. Each file under `prompts/` is a self-contained user message you can paste as the first turn of a fresh Claude Code session opened inside this directory. Each prompt directs the session to produce its output under `tests/<NN>-<slug>/`.

See [CLAUDE.md](CLAUDE.md) for the test-session contract (what the session is allowed to touch, what to flag when the skill misbehaves) and [prompts/README.md](prompts/README.md) for **what each prompt is actually testing** — the skill conventions we expect to see applied unprompted, the references the skill should route to, and the anti-patterns that count as a failure even if the produced code "works."

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

## Running a test

The wrapper `tests/run-prompt.sh` handles env-isolation and (optionally) transcript capture:

```bash
cd <repo-root>/tests   # or, while iterating, the worktree's tests/

# Interactive (recommended for live observation): you watch routing + tool-use happen.
# CLAUDE_CODE_SKIP_PROMPT_HISTORY=1 is auto-set so the run doesn't pollute ~/.claude.
./run-prompt.sh 04-debug-delete-button
# Then in the running session, paste the contents of prompts/04-debug-delete-button.md
# (use your clipboard / editor — claude pipes stdin into print mode, no clean
# "pre-fill the prompt and stay interactive" flag exists).

# Non-interactive (archival): captures the full transcript locally as JSONL.
./run-prompt.sh 04-debug-delete-button --capture
# Transcript saved to tests/transcripts/04-debug-delete-button__<UTC>.jsonl
```

Output paths in the prompts are **relative to your cwd (`tests/`)**. So a prompt that says `04-debug-delete-button/response.md` resolves to `tests/04-debug-delete-button/response.md` on disk.

After the run:

1. Inspect the produced `<NN>-<slug>/` subdir for the artifacts.
2. If you used `--capture`, open the JSONL transcript and grade routing + tool use against `prompts/README.md`'s rubric.
3. Note misbehavior either in a Linear `friction` issue, or as a new test case under `evals/`, or both.

### Manual equivalent (skip the wrapper)

```bash
cd <repo-root>/tests
CLAUDE_CODE_SKIP_PROMPT_HISTORY=1 claude              # interactive
# or, archival:
CLAUDE_CODE_SKIP_PROMPT_HISTORY=1 claude -p "$(cat prompts/<NN>-<slug>.md)" \
    --output-format stream-json \
    > transcripts/<NN>-<slug>__$(date -u +%Y%m%dT%H%M%SZ).jsonl 2>&1
```

### History isolation

`CLAUDE_CODE_SKIP_PROMPT_HISTORY=1` (the wrapper sets it) keeps test runs out of:

- `~/.claude/history.jsonl` — the global recent-prompts log you scan for insights/friction.
- `~/.claude/projects/<encoded>/<session>.jsonl` — per-project session transcripts.

Trade-off: with the flag set, Claude Code does not write a per-project transcript either. That's fine for `--capture` (we get our own JSONL). For interactive mode it means you observe live but no auto-recording — if you want a recording of an interactive run, do a `--capture` pass afterward, or screen-capture, or run twice (once each).

Without the wrapper, raw `claude` invocations from `tests/` will record prompts under `~/.claude/history.jsonl` with `project` set to the tests cwd — easy to `jq`-filter when scanning, but they're there.

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
