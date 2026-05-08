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

Single Python entry point — `tests/run-prompt.py`. Runs the prompt non-interactively via `claude -p`, streams output live, saves a transcript:

```bash
cd <repo-root>/tests   # or the worktree's tests/ while iterating

# Default: text output, streamed to terminal AND saved to transcripts/<NN>-<slug>__<UTC>.txt
./run-prompt.py 04-debug-delete-button

# Stream-json with live pretty-printing (thinking blocks, tool uses, results):
./run-prompt.py 04-debug-delete-button --json
# raw JSONL saved to transcripts/04-debug-delete-button__<UTC>.jsonl
```

What it does:

- Echoes the prompt up front so the transcript is self-contained.
- Sets `CLAUDE_CODE_SKIP_PROMPT_HISTORY=1` so the run doesn't pollute `~/.claude/history.jsonl` or the per-project transcript dir.
- Streams `claude -p`'s output to your terminal AND `transcripts/<NN>-<slug>__<UTC>.{txt,jsonl}`. Both transcript extensions are gitignored, so you only commit transcripts intentionally (e.g. as evidence of a memorable failure).
- In `--json` mode, pretty-prints events to the terminal (assistant prose streams inline, thinking blocks shown wrapped under 💭, tool uses as 🔧 [name], tool results as ←, system hook noise suppressed) while writing the *raw* JSONL stream to disk for later grading.

Output paths in the prompts are **relative to your cwd (`tests/`)**. A prompt that says `04-debug-delete-button/response.md` resolves to `tests/04-debug-delete-button/response.md` on disk.

After the run:

1. Inspect the produced `<NN>-<slug>/` subdir for the artifacts.
2. Grade routing decisions and idiom adoption against `prompts/README.md`'s rubric. The `--json` transcript makes tool-use grading much easier than reading text.
3. Note misbehavior in a Linear `friction` issue or as a new test case under `evals/`, or both.

### Manual equivalent (skip the wrapper)

```bash
cd <repo-root>/tests
ts=$(date -u +%Y%m%dT%H%M%SZ)
# text mode:
CLAUDE_CODE_SKIP_PROMPT_HISTORY=1 claude -p "$(cat prompts/<NN>-<slug>.md)" 2>&1 \
    | tee transcripts/<NN>-<slug>__${ts}.txt
# stream-json (raw JSONL, no pretty-printing):
CLAUDE_CODE_SKIP_PROMPT_HISTORY=1 claude -p "$(cat prompts/<NN>-<slug>.md)" \
    --output-format stream-json --verbose 2>&1 \
    | tee transcripts/<NN>-<slug>__${ts}.jsonl
```

The wrapper is just a thin orchestration layer over the above plus the JSON pretty-printer; if you want to inspect or modify the live view, edit `run-prompt.py`.

### History isolation

`CLAUDE_CODE_SKIP_PROMPT_HISTORY=1` (set by the wrapper) keeps test runs out of:

- `~/.claude/history.jsonl` — the global recent-prompts log you scan for insights/friction.
- `~/.claude/projects/<encoded>/<session>.jsonl` — per-project session transcripts.

There's no env var that redirects only history while keeping plugins/settings in `~/.claude/`. Skip-and-capture-locally is the cleanest compromise.

### Plugin install scope (heads-up)

For test sessions to see `imgui-cpp` while running from this worktree, the plugin must be installed with **user scope**, not local scope. Local scope keys on `git rev-parse --show-toplevel` of the cwd, and a worktree's toplevel is the worktree path — not the original repo — so a local-scope install is silently invisible from worktrees of the same project. (Tracked upstream as Linear MAIN-19.)

User-scope means the plugin auto-activates in **every** Claude Code session on this machine, including unrelated ImGui projects. That's deliberate during this dev cycle, but it's the wrong long-term state — an in-flight version of the plugin shouldn't be silently routing in your unrelated work. **At the end of the iteration, revert to local scope or uninstall:** see Linear MAIN-20 for the explicit checklist and the `/plugin uninstall + /plugin install` sequence.

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
