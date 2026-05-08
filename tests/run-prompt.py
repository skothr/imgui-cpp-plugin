#!/usr/bin/env python3
"""run-prompt.py — run a tests/prompts/<NN>-<slug>.md prompt as a non-interactive
Claude Code session, stream output live to terminal, save a transcript, and
ALWAYS test against the latest plugin source (no install/uninstall dance).

Usage:
    ./run-prompt.py <slug>           # text output (default)
    ./run-prompt.py <slug> --json    # stream-json with pretty live view
    ./run-prompt.py 04-debug-delete-button
    ./run-prompt.py 04-debug-delete-button --json

What it does:

- Auto-discovers the plugin source root by walking up from this script
  looking for the directory containing `.claude-plugin/`. Passes that path
  to `claude --plugin-dir`, which loads the plugin for THIS SESSION ONLY
  from the live source files. No global install needed; no caching, no
  cwd-vs-projectPath worries (Linear MAIN-19), no leftover state to clean
  up afterward (Linear MAIN-20). Every run picks up the freshest source.
- Reads `tests/prompts/<slug>.md` and feeds it to `claude -p` as a one-shot
  non-interactive session. cwd is `tests/` so the session reads
  `tests/CLAUDE.md` and writes its output subdirs there.
- Sets `CLAUDE_CODE_SKIP_PROMPT_HISTORY=1` so the run does not pollute
  `~/.claude/history.jsonl` or the per-project transcript dir.
- Echoes the prompt up front (to terminal + transcript) so the saved file is
  self-contained.
- Streams claude's output live; the full raw output is appended to
  `tests/transcripts/<slug>__<UTC>.{txt|jsonl}` (gitignored).
- In `--json` mode, the raw JSONL stream is pretty-printed for the live view:
  assistant prose streams inline, thinking blocks shown wrapped under
  `[thinking]`, tool uses as `[tool: name]`, tool results as `[tool_result]`,
  system hook noise suppressed. Saves the *raw* JSONL to disk for later
  grading.

Honest trade-offs:

- `--json` requires `--verbose` in Claude Code 2.1+ (handled here).
- With `CLAUDE_CODE_SKIP_PROMPT_HISTORY=1` set, Claude Code does not write a
  per-project transcript either; the saved file under `transcripts/` is the
  only recording.
- xhigh effort + Opus may take 30-60s before any visible output streams in
  text mode (thinking events aren't surfaced). `--json` shows thinking live.
- `--plugin-dir` loads the plugin per-session only. If you ALSO have a
  globally-installed copy of the same plugin, the per-session one wins for
  this run, but the global state is unchanged.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

# ---------------------------------------------------------------------------
# Pretty-print stream-json events for live terminal viewing.
# ---------------------------------------------------------------------------

WRAP_WIDTH = 100
SUMMARY_MAX = 400

# System events that are pure noise on a normal session and should be hidden.
SYSTEM_SUPPRESS = {"hook_started", "hook_response", "init"}


def _summarize(value, max_len: int = SUMMARY_MAX) -> str:
    if isinstance(value, (dict, list)):
        s = json.dumps(value, ensure_ascii=False)
    else:
        s = str(value)
    if len(s) <= max_len:
        return s
    return s[:max_len] + f"…[truncated, {len(s) - max_len} more chars]"


def _emit(text: str) -> None:
    sys.stdout.write(text)
    sys.stdout.flush()


def _emit_block(label: str, body: str = "") -> None:
    _emit("\n" + label + (" " + body if body else "") + "\n")


def _emit_thinking(thinking: str) -> None:
    if not thinking.strip():
        return
    _emit("\n[thinking] -------------------------------------------\n")
    for paragraph in thinking.split("\n"):
        if not paragraph.strip():
            _emit("\n")
            continue
        wrapped_lines = textwrap.wrap(
            paragraph,
            width=WRAP_WIDTH,
            replace_whitespace=False,
            drop_whitespace=False,
        ) or [""]
        for w in wrapped_lines:
            _emit("   " + w + "\n")
    _emit("------------------------------------------------------\n")


def _handle_assistant(ev: dict) -> None:
    for block in (ev.get("message") or {}).get("content") or []:
        bt = block.get("type")
        if bt == "text":
            _emit(block.get("text", ""))
        elif bt == "thinking":
            _emit_thinking(block.get("thinking", ""))
        elif bt == "tool_use":
            _emit_block(
                f"[tool: {block.get('name', '?')}]",
                _summarize(block.get("input", {})),
            )
        elif bt == "tool_result":  # defensive — usually appears under user
            _emit_block("[tool_result]", _summarize(block.get("content", "")))


def _handle_user(ev: dict) -> None:
    for block in (ev.get("message") or {}).get("content") or []:
        if block.get("type") != "tool_result":
            continue
        content = block.get("content", "")
        if isinstance(content, list):
            content = " ".join(
                c.get("text", json.dumps(c, ensure_ascii=False))
                if isinstance(c, dict)
                else str(c)
                for c in content
            )
        _emit_block("[tool_result]", _summarize(content))


def _handle_result(ev: dict) -> None:
    parts = []
    if (cost := ev.get("total_cost_usd")) is not None:
        parts.append(f"cost=${cost}")
    if (duration_ms := ev.get("duration_ms")) is not None:
        parts.append(f"duration={duration_ms}ms")
    if (turns := ev.get("num_turns")) is not None:
        parts.append(f"turns={turns}")
    _emit("\n\n=== DONE === " + "  ".join(parts) + "\n")


def _handle_system(ev: dict) -> None:
    sub = ev.get("subtype", "")
    if sub in SYSTEM_SUPPRESS:
        return
    _emit_block(f"[system:{sub or '?'}]")


_HANDLERS = {
    "assistant": _handle_assistant,
    "user": _handle_user,
    "result": _handle_result,
    "system": _handle_system,
}


def _render_event(line: str) -> None:
    """Pretty-print a single JSONL line. Non-JSON lines pass through."""
    line = line.rstrip("\n")
    if not line.strip():
        return
    try:
        ev = json.loads(line)
    except json.JSONDecodeError:
        _emit(line + "\n")
        return
    handler = _HANDLERS.get(ev.get("type"))
    if handler is not None:
        try:
            handler(ev)
        except Exception as exc:  # noqa: BLE001
            _emit_block(f"[pretty-stream error in {ev.get('type')}]", str(exc))


# ---------------------------------------------------------------------------
# Driver: spawn claude -p, fan out to terminal + transcript file.
# ---------------------------------------------------------------------------


def _list_available_prompts(tests_dir: Path) -> None:
    prompts_dir = tests_dir / "prompts"
    print("available prompts:", file=sys.stderr)
    for p in sorted(prompts_dir.glob("*.md")):
        if p.name == "README.md":
            continue
        print(f"  {p.stem}", file=sys.stderr)


def _find_plugin_root(start: Path) -> Path:
    """Walk up from `start` looking for the dir that contains `.claude-plugin/`.

    For tests/run-prompt.py, this resolves to the plugin's source root —
    typically the worktree root if we're in a worktree, or the repo root
    otherwise.
    """
    current = start.resolve()
    while True:
        if (current / ".claude-plugin").is_dir():
            return current
        if current == current.parent:
            raise RuntimeError(
                "couldn't locate .claude-plugin/ above run-prompt.py — "
                "is this script inside a Claude Code plugin tree?"
            )
        current = current.parent


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run a tests/prompts/<slug>.md prompt as a non-interactive Claude Code "
            "session. Streams output live, saves a transcript, isolates from "
            "~/.claude history."
        ),
    )
    parser.add_argument("prompt", help="prompt slug, e.g. 04-debug-delete-button")
    parser.add_argument(
        "--json",
        dest="json_mode",
        action="store_true",
        help=(
            "use --output-format stream-json with live pretty-printing — shows "
            "thinking blocks, tool uses, and tool results inline. Default: text."
        ),
    )
    args = parser.parse_args()

    # tests/run-prompt.py -> tests/
    tests_dir = Path(__file__).resolve().parent
    prompt_file = tests_dir / "prompts" / f"{args.prompt}.md"
    if not prompt_file.is_file():
        print(f"no such prompt: {prompt_file}", file=sys.stderr)
        _list_available_prompts(tests_dir)
        return 1

    if shutil.which("claude") is None:
        print(
            "claude binary not found on PATH — install Claude Code or check $PATH",
            file=sys.stderr,
        )
        return 1

    try:
        plugin_root = _find_plugin_root(tests_dir)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    transcripts_dir = tests_dir / "transcripts"
    transcripts_dir.mkdir(exist_ok=True)
    ts = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    ext = "jsonl" if args.json_mode else "txt"
    out_path = transcripts_dir / f"{args.prompt}__{ts}.{ext}"

    # Self-contained transcript: prompt up front, then claude's output.
    prompt_body = prompt_file.read_text()
    header = (
        f"=== PROMPT ({args.prompt}) ===\n"
        f"{prompt_body}\n"
        f"=== CLAUDE ({'stream-json' if args.json_mode else 'text'}, {ts}) ===\n"
    )
    sys.stdout.write(header)
    sys.stdout.flush()
    out_path.write_text(header)

    print(
        f"[..] loading plugin from source: {plugin_root}",
        file=sys.stderr,
    )
    print(
        "[..] thinking phase may run 30-60s before any prose streams; "
        "--json shows live thinking + tool use events",
        file=sys.stderr,
    )

    env = os.environ.copy()
    env["CLAUDE_CODE_SKIP_PROMPT_HISTORY"] = "1"

    cmd = [
        "stdbuf", "-oL",
        "claude", "-p", prompt_body,
        "--plugin-dir", str(plugin_root),
    ]
    if args.json_mode:
        # claude -p --output-format stream-json requires --verbose in 2.1+.
        cmd += ["--output-format", "stream-json", "--verbose"]

    try:
        proc = subprocess.Popen(
            cmd,
            cwd=str(tests_dir),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # merge claude stderr into the same stream
            bufsize=1,                 # line-buffered for the parent pipe read
            text=True,
        )
    except FileNotFoundError as exc:
        # stdbuf missing? rare, but fall back to no stdbuf.
        print(f"warning: {exc}; retrying without stdbuf wrapper", file=sys.stderr)
        cmd = cmd[2:]
        proc = subprocess.Popen(
            cmd, cwd=str(tests_dir), env=env,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=1, text=True,
        )

    # stdout=subprocess.PIPE guarantees a real stream here at runtime.
    assert proc.stdout is not None

    # Fan out: every line from claude → append to transcript file AND
    # render to stdout (raw in text mode, pretty in JSON mode).
    with out_path.open("a") as out_fh:
        try:
            for line in proc.stdout:
                out_fh.write(line)
                out_fh.flush()
                if args.json_mode:
                    _render_event(line)
                else:
                    sys.stdout.write(line)
                    sys.stdout.flush()
        finally:
            proc.wait()

    print(f"\n· transcript saved to {out_path}", file=sys.stderr)
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
