#!/usr/bin/env python3
"""run-prompt.py — run a tests/prompts/<NN>-<slug>.md prompt as a non-interactive
Claude Code session, stream output live to terminal, save a transcript, and
ALWAYS test against the latest plugin source (no install/uninstall dance).

Usage:
    ./run-prompt.py <ref>                  # text output (default)
    ./run-prompt.py <ref> --json           # stream-json with pretty live view
    ./run-prompt.py 04                     # numeric ref — resolves to "04-...".md
    ./run-prompt.py 4                      # leading zero optional
    ./run-prompt.py 04-debug-delete-button # full slug also works
    ./run-prompt.py 05 06 07 --json        # multiple prompts, run in order

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

# Event types we keep in the JSONL transcript. Stream events (token-level
# deltas from --include-partial-messages) are deliberately excluded — they're
# a live-view concern, and emitting every text-delta would balloon the
# .jsonl by ~100x and make downstream parsing miserable. The transcript
# stays at message granularity; the live terminal view is where partial
# tokens render.
JSONL_KEEP_TYPES = {"assistant", "user", "result"}


def _should_keep_in_jsonl(ev: dict) -> bool:
    et = ev.get("type")
    if et == "system":
        return ev.get("subtype", "") not in SYSTEM_SUPPRESS
    return et in JSONL_KEEP_TYPES


# Per-block bookkeeping for partial-message streaming. Keyed by (message_id,
# block_index). Reset implicitly per message — message_start clears stale
# state from prior turns by the time a new index 0 arrives. We track:
#   _partial_active: block_type currently open at that key (so content_block_stop
#                    can emit the right close marker for thinking blocks).
#   _partial_streamed: blocks that received at least one text/thinking delta.
#                      Consulted by _handle_assistant to skip re-emitting
#                      content the user already saw stream past.
_partial_active: dict[tuple[str, int], str] = {}
_partial_streamed: set[tuple[str, int]] = set()


def _reset_partial_state() -> None:
    """Clear cross-run state. Different prompts get different message ids
    so the dedupe set wouldn't *practically* collide, but resetting is
    defensive and keeps memory bounded across long multi-prompt runs."""
    _partial_active.clear()
    _partial_streamed.clear()


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
    msg_id = (ev.get("message") or {}).get("id", "")
    for idx, block in enumerate((ev.get("message") or {}).get("content") or []):
        bt = block.get("type")
        # If --include-partial-messages was on, the text/thinking content for
        # this block already streamed past as deltas. Emitting the full block
        # again would duplicate it. Tool-use blocks still get emitted here:
        # input_json deltas don't render as readable JSON mid-stream, so the
        # assembled tool-use input at message level is the human-friendly view.
        already_streamed = (msg_id, idx) in _partial_streamed
        if bt == "text":
            if not already_streamed:
                _emit(block.get("text", ""))
        elif bt == "thinking":
            if not already_streamed:
                _emit_thinking(block.get("thinking", ""))
        elif bt == "tool_use":
            _emit_block(
                f"[tool: {block.get('name', '?')}]",
                _summarize(block.get("input", {})),
            )
        elif bt == "tool_result":  # defensive — usually appears under user
            _emit_block("[tool_result]", _summarize(block.get("content", "")))


def _handle_stream_event(ev: dict) -> None:
    """Render token-level deltas from --include-partial-messages.

    Event shape (mirrors the Anthropic SSE format):
        {"type":"stream_event",
         "event":{"type":"content_block_delta","index":N,
                  "delta":{"type":"text_delta","text":"..."}}}

    We render text_delta and thinking_delta inline so the user sees prose
    appear character-by-character. input_json_delta (tool-use input
    streaming) is skipped — the assembled tool_use comes through later as
    a message-level event and renders cleanly there. signature_delta and
    citations_delta are also skipped.
    """
    inner = ev.get("event") or {}
    et = inner.get("type")
    msg_id = (inner.get("message") or {}).get("id") or ev.get("session_id", "")
    idx = inner.get("index", 0)
    key = (msg_id, idx)

    if et == "content_block_start":
        bt = (inner.get("content_block") or {}).get("type", "")
        _partial_active[key] = bt
        if bt == "thinking":
            # Open the indented thinking frame; subsequent thinking_deltas
            # write inside it. Newlines in deltas are converted to "\n   "
            # so multi-line thinking stays indented under the frame.
            _emit("\n[thinking] -------------------------------------------\n   ")
    elif et == "content_block_delta":
        delta = inner.get("delta") or {}
        dt = delta.get("type")
        if dt == "text_delta":
            _emit(delta.get("text", ""))
            _partial_streamed.add(key)
        elif dt == "thinking_delta":
            text = delta.get("thinking", "")
            # Indent continuations to match the opening frame.
            _emit(text.replace("\n", "\n   "))
            _partial_streamed.add(key)
        # input_json_delta / signature_delta / citations_delta: ignored.
    elif et == "content_block_stop":
        bt = _partial_active.pop(key, None)
        if bt == "thinking":
            _emit("\n------------------------------------------------------\n")
        elif bt == "text":
            _emit("\n")
    # message_start / message_delta / message_stop: ignored. The
    # message-level "assistant" event (with finalized blocks) follows
    # and _handle_assistant consults _partial_streamed to dedupe.


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
    "stream_event": _handle_stream_event,
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


def _iter_prompt_paths(tests_dir: Path):
    for p in sorted((tests_dir / "prompts").glob("*.md")):
        if p.name == "README.md":
            continue
        yield p


def _list_available_prompts(tests_dir: Path, *, to_stdout: bool = False) -> None:
    out = sys.stdout if to_stdout else sys.stderr
    if not to_stdout:
        print("available prompts:", file=out)
    for p in _iter_prompt_paths(tests_dir):
        # Show first non-empty line of the body as a one-line summary so the
        # listing is browsable without opening every file.
        summary = ""
        for raw in p.read_text().splitlines():
            line = raw.strip()
            if line and not line.startswith("#"):
                summary = line[:80] + ("..." if len(line) > 80 else "")
                break
        prefix = "" if to_stdout else "  "
        if summary:
            print(f"{prefix}{p.stem:<28} {summary}", file=out)
        else:
            print(f"{prefix}{p.stem}", file=out)


def _resolve_prompt(tests_dir: Path, ref: str) -> Path:
    """Resolve a user-supplied prompt reference to a prompts/<slug>.md file.

    Accepts three forms:
      - exact slug:   "04-debug-delete-button"
      - with .md:     "04-debug-delete-button.md"
      - numeric:      "04" or "4" (zero-padded to two digits, then matched
                                   against the "NN-" prefix of available
                                   prompt stems)

    Raises FileNotFoundError if nothing matches, ValueError if a numeric
    ref is ambiguous (e.g., "1" matching both "1-foo" and "10-bar" — not
    expected with the current 2-digit convention, but handled defensively).
    """
    prompts_dir = tests_dir / "prompts"
    available = sorted(
        p for p in prompts_dir.glob("*.md") if p.name != "README.md"
    )
    stems = {p.stem: p for p in available}

    if ref.endswith(".md"):
        ref = ref[:-3]

    if ref in stems:
        return stems[ref]

    if ref.isdigit():
        padded = ref.zfill(2)
        matches = [p for stem, p in stems.items() if stem.startswith(padded + "-")]
        if len(matches) == 1:
            return matches[0]
        if not matches:
            raise FileNotFoundError(f"no prompt with prefix '{padded}-'")
        raise ValueError(
            f"ambiguous numeric ref '{ref}'; matches: "
            f"{sorted(m.stem for m in matches)}"
        )

    raise FileNotFoundError(f"no prompt matching '{ref}'")


def _show_prompt(tests_dir: Path, ref: str) -> int:
    try:
        f = _resolve_prompt(tests_dir, ref)
    except (FileNotFoundError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        _list_available_prompts(tests_dir)
        return 1
    sys.stdout.write(f.read_text())
    if not f.read_text().endswith("\n"):
        sys.stdout.write("\n")
    return 0


def _latest_transcript(tests_dir: Path, ref: str) -> int:
    # Reuse the resolver so numeric refs work here too.
    try:
        prompt_file = _resolve_prompt(tests_dir, ref)
    except (FileNotFoundError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        _list_available_prompts(tests_dir)
        return 1
    slug = prompt_file.stem
    candidates = sorted(
        (tests_dir / "transcripts").glob(f"{slug}__*"),
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )
    if not candidates:
        print(f"no transcripts found for prompt: {slug}", file=sys.stderr)
        return 1
    print(candidates[0])
    return 0


def _archive_existing_output(tests_dir: Path, slug: str, ts: str) -> Path | None:
    """Move tests/<slug>/ to tests/archived/<slug>__<ts>/ if it exists.

    Returns the archive destination on success, or None if nothing was
    archived (the prompt either doesn't produce a project tree at all,
    or this is the first run for the slug). The timestamp is the same
    one used for the new run's transcript filename, so:

        transcripts/<slug>__<ts>.jsonl   <- the run starting at <ts>
        archived/<slug>__<ts>/           <- the prior output displaced by it

    Same filesystem rename, so it's atomic. If the archive destination
    somehow exists already (same-second collision), append a -N suffix.
    """
    existing = tests_dir / slug
    if not existing.is_dir():
        return None  # diagnostic prompt or first run; nothing to archive.

    archive_root = tests_dir / "archived"
    archive_root.mkdir(exist_ok=True)

    dest = archive_root / f"{slug}__{ts}"
    counter = 0
    while dest.exists():
        counter += 1
        dest = archive_root / f"{slug}__{ts}-{counter}"

    existing.rename(dest)
    return dest


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
            "session, or inspect the prompt set / past transcripts."
        ),
    )

    # Mode flags (mutually exclusive). If none set, we run the prompt.
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "-l", "--list",
        dest="list_prompts",
        action="store_true",
        help="list available prompts and exit",
    )
    mode.add_argument(
        "--show",
        metavar="PROMPT",
        help="print the body of PROMPT and exit (no run)",
    )
    mode.add_argument(
        "--latest",
        metavar="PROMPT",
        help="print the path to the most recent transcript for PROMPT and exit",
    )

    parser.add_argument(
        "prompts",
        nargs="*",
        metavar="REF",
        help=(
            "prompt(s) to run. Accepts the full slug "
            "('04-debug-delete-button'), a numeric ref ('04' or '4'), or a "
            "slug with '.md'. Pass multiple to run a batch in order; each "
            "produces its own transcript file."
        ),
    )
    parser.add_argument(
        "--json",
        dest="json_mode",
        action="store_true",
        help=(
            "use --output-format stream-json with live pretty-printing (shows "
            "thinking blocks, tool uses, tool results); default: text"
        ),
    )
    parser.add_argument(
        "--no-partial",
        dest="partial",
        action="store_false",
        help=(
            "disable token-level streaming. Default: --json mode passes "
            "--include-partial-messages to claude so prose and thinking "
            "render character-by-character. Disable for older claude "
            "versions that don't support the flag, or to debug message-level "
            "event flow without delta noise. Ignored in text mode."
        ),
    )
    parser.add_argument(
        "--effort",
        choices=["low", "medium", "high", "xhigh", "max"],
        help=(
            "override thinking effort for this run (passes --effort to claude). "
            "Default: whatever your CLAUDE_EFFORT env / settings define."
        ),
    )
    parser.add_argument(
        "--plugin-dir",
        dest="plugin_dir",
        metavar="PATH",
        help=(
            "override the auto-detected plugin source root. Default: walks up "
            "from this script to the dir containing .claude-plugin/. Useful if "
            "you want to test against a different worktree, or against the "
            "plugin source on main from inside a feature worktree."
        ),
    )
    parser.add_argument(
        "--no-capture",
        dest="capture",
        action="store_false",
        help="don't save a transcript to transcripts/ (default: save)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print the claude command that would run, then exit; don't run it",
    )
    args = parser.parse_args()

    tests_dir = Path(__file__).resolve().parent

    # Mode dispatch — these short-circuit the run path.
    if args.list_prompts:
        _list_available_prompts(tests_dir, to_stdout=True)
        return 0
    if args.show:
        return _show_prompt(tests_dir, args.show)
    if args.latest:
        return _latest_transcript(tests_dir, args.latest)

    # From here on, we're running prompt(s) — at least one ref required.
    if not args.prompts:
        parser.error("prompt ref required (or use --list / --show / --latest)")

    # Validate claude binary once unless we're only doing a dry-run.
    if not args.dry_run and shutil.which("claude") is None:
        print(
            "claude binary not found on PATH — install Claude Code or check $PATH",
            file=sys.stderr,
        )
        return 1

    # Resolve plugin root once; all prompts share it.
    if args.plugin_dir:
        plugin_root = Path(args.plugin_dir).expanduser().resolve()
        if not (plugin_root / ".claude-plugin").is_dir():
            print(
                f"--plugin-dir {plugin_root}: no .claude-plugin/ subdir found",
                file=sys.stderr,
            )
            return 1
    else:
        try:
            plugin_root = _find_plugin_root(tests_dir)
        except RuntimeError as exc:
            print(str(exc), file=sys.stderr)
            return 1

    # Resolve every ref up front so a typo on prompt #3 fails before we
    # spend three minutes running prompts #1 and #2.
    resolved: list[Path] = []
    for ref in args.prompts:
        try:
            resolved.append(_resolve_prompt(tests_dir, ref))
        except (FileNotFoundError, ValueError) as exc:
            print(f"error resolving '{ref}': {exc}", file=sys.stderr)
            _list_available_prompts(tests_dir)
            return 1

    total = len(resolved)
    worst_rc = 0
    for idx, prompt_file in enumerate(resolved, start=1):
        if total > 1:
            banner = f"\n========== [{idx}/{total}] {prompt_file.stem} ==========\n"
            sys.stderr.write(banner)
            sys.stderr.flush()
        rc = _run_one(args, prompt_file, plugin_root, tests_dir)
        if rc != 0 and worst_rc == 0:
            worst_rc = rc
        # Even on non-zero, continue to the next prompt. The user asked for
        # a batch; one failure doesn't invalidate the others' transcripts.

    if total > 1:
        summary = f"\n========== batch done: {total} prompt(s), worst exit {worst_rc} ==========\n"
        sys.stderr.write(summary)
    return worst_rc


def _run_one(args, prompt_file: Path, plugin_root: Path, tests_dir: Path) -> int:
    """Run a single prompt. Extracted from main() so the batch loop is clean.

    Returns the claude subprocess's exit code (0 on success), or 0 on --dry-run.
    """
    slug = prompt_file.stem
    ts = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    ext = "jsonl" if args.json_mode else "txt"

    out_path: Path | None = None
    if args.capture:
        transcripts_dir = tests_dir / "transcripts"
        transcripts_dir.mkdir(exist_ok=True)
        out_path = transcripts_dir / f"{slug}__{ts}.{ext}"

    prompt_body = prompt_file.read_text()

    cmd = [
        "stdbuf", "-oL",
        "claude", "-p", prompt_body,
        "--plugin-dir", str(plugin_root),
    ]
    if args.json_mode:
        # claude -p --output-format stream-json requires --verbose in 2.1+.
        cmd += ["--output-format", "stream-json", "--verbose"]
        if args.partial:
            # Token-level streaming. Without this flag, stream-json emits
            # one event per complete assistant message — fine for the JSONL
            # transcript but the live terminal view freezes between turns.
            cmd += ["--include-partial-messages"]
    if args.effort:
        cmd += ["--effort", args.effort]

    # --dry-run: show the resolved command and exit. stdbuf wrapper included
    # so what you see is what would actually run. Also report what *would*
    # be archived so the user can see the side effect before it happens.
    if args.dry_run:
        print("would run:", file=sys.stderr)
        print("  cwd: " + str(tests_dir), file=sys.stderr)
        print("  env: CLAUDE_CODE_SKIP_PROMPT_HISTORY=1", file=sys.stderr)
        print("  cmd: " + " ".join(
            f"'{a}'" if " " in a or "\n" in a else a for a in cmd
        ), file=sys.stderr)
        if out_path is not None:
            print("  out: " + str(out_path), file=sys.stderr)
        if (tests_dir / slug).is_dir():
            print(
                f"  archive: tests/{slug}/ -> tests/archived/{slug}__{ts}/",
                file=sys.stderr,
            )
        return 0

    # Archive any prior output for this slug before the run starts. The
    # new run will write into tests/<slug>/ from scratch; the previous
    # tree lives at tests/archived/<slug>__<ts>/ matching this run's
    # transcript timestamp. No-op for diagnostic prompts that never
    # produce a project tree.
    archived = _archive_existing_output(tests_dir, slug, ts)
    if archived is not None:
        print(f"[..] archived prior output to {archived}", file=sys.stderr)

    # Each run gets a fresh streaming state so block keys from a prior
    # prompt's message ids can't accidentally dedupe content in this one.
    _reset_partial_state()

    # Terminal header is plain text either way (it's for the human watching).
    human_header = (
        f"=== PROMPT ({slug}) ===\n"
        f"{prompt_body}\n"
        f"=== CLAUDE ({'stream-json' if args.json_mode else 'text'}, {ts}) ===\n"
    )
    sys.stdout.write(human_header)
    sys.stdout.flush()

    # Transcript file: in TEXT mode, mirror the human header for a
    # self-contained log. In JSONL mode, keep the file strictly valid
    # JSONL by writing a synthetic test_meta event as the first line.
    if out_path is not None:
        if args.json_mode:
            meta_event = {
                "type": "test_meta",
                "prompt_slug": slug,
                "prompt_body": prompt_body,
                "started_at_utc": ts,
                "format": "stream-json",
                "plugin_root": str(plugin_root),
                "effort": args.effort,
            }
            out_path.write_text(
                json.dumps(meta_event, ensure_ascii=False) + "\n"
            )
        else:
            out_path.write_text(human_header)

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

    # Stream from claude. In TEXT mode every line goes to both terminal and
    # the .txt file. In JSON mode the terminal gets the pretty-printed view
    # while the .jsonl file gets only events the user actually saw —
    # assistant turns, user tool_results, the final result, and any
    # non-suppressed system events. Hook noise is dropped in both views,
    # but the file is also kept strictly valid JSONL (no plain headers).
    # If --no-capture, out_path is None and we just stream to terminal.
    out_fh = out_path.open("a") if out_path is not None else None
    try:
        try:
            for line in proc.stdout:
                if args.json_mode:
                    try:
                        ev = json.loads(line)
                    except json.JSONDecodeError:
                        ev = None
                    if (
                        out_fh is not None
                        and ev is not None
                        and _should_keep_in_jsonl(ev)
                    ):
                        out_fh.write(line)
                        out_fh.flush()
                    _render_event(line)
                else:
                    if out_fh is not None:
                        out_fh.write(line)
                        out_fh.flush()
                    sys.stdout.write(line)
                    sys.stdout.flush()
        finally:
            proc.wait()

        # Tag the test boundary so downstream tools can spot a clean end.
        if out_fh is not None and args.json_mode:
            end_event = {"type": "test_end", "exit_code": proc.returncode}
            out_fh.write(json.dumps(end_event) + "\n")
    finally:
        if out_fh is not None:
            out_fh.close()

    if out_path is not None:
        print(f"\n[..] transcript saved to {out_path}", file=sys.stderr)
    else:
        print("\n[..] no transcript saved (--no-capture)", file=sys.stderr)
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
