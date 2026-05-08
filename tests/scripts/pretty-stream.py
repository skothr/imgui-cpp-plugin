#!/usr/bin/env python3
"""Pretty-print Claude Code stream-json events from stdin to stdout.

Reads one JSON event per line and emits a human-readable view:
  - assistant text blocks → printed inline (the actual response prose)
  - assistant thinking blocks → shown wrapped, prefixed with 💭
  - assistant tool_use blocks → shown as [tool: name](input-summary)
  - user tool_result blocks → shown as [tool_result] summary
  - result events → final cost/duration line
  - system events (hook_started / hook_response / etc) → suppressed (noise)

Designed to be the last stage in `claude -p --output-format stream-json | tee
file.jsonl | pretty-stream.py` — the file gets the full raw stream, this
process renders the live human view.

Output is line-buffered (sys.stdout flushed on every print) so the live
terminal streams as events arrive.
"""

from __future__ import annotations
import json
import sys
import textwrap


# Width to wrap long thinking blocks at (characters).
WRAP_WIDTH = 100
# Truncate tool_use input / tool_result content past this length.
SUMMARY_MAX = 400


def summarize(value, max_len: int = SUMMARY_MAX) -> str:
    """Render value as a single-line string, truncated."""
    if isinstance(value, (dict, list)):
        s = json.dumps(value, ensure_ascii=False)
    else:
        s = str(value)
    if len(s) <= max_len:
        return s
    return s[:max_len] + f"…[truncated, {len(s) - max_len} more chars]"


def emit_text(text: str) -> None:
    """Stream assistant prose verbatim."""
    sys.stdout.write(text)
    sys.stdout.flush()


def emit_block(label: str, body: str = "") -> None:
    """Emit a labeled block on its own line(s)."""
    sys.stdout.write("\n" + label)
    if body:
        sys.stdout.write(" " + body)
    sys.stdout.write("\n")
    sys.stdout.flush()


def emit_thinking(thinking: str) -> None:
    """Show thinking content wrapped, with a 💭 prefix."""
    if not thinking.strip():
        return
    sys.stdout.write("\n💭 thinking ─────────────────────────────────────────\n")
    for paragraph in thinking.split("\n"):
        if not paragraph.strip():
            sys.stdout.write("\n")
            continue
        for wrapped in textwrap.wrap(
            paragraph, width=WRAP_WIDTH, replace_whitespace=False, drop_whitespace=False
        ) or [""]:
            sys.stdout.write("   " + wrapped + "\n")
    sys.stdout.write("─────────────────────────────────────────────────────\n")
    sys.stdout.flush()


def handle_assistant(ev: dict) -> None:
    """Emit blocks from an assistant turn."""
    msg = ev.get("message", {})
    for block in msg.get("content") or []:
        bt = block.get("type")
        if bt == "text":
            emit_text(block.get("text", ""))
        elif bt == "thinking":
            emit_thinking(block.get("thinking", ""))
        elif bt == "tool_use":
            name = block.get("name", "?")
            input_summary = summarize(block.get("input", {}))
            emit_block(f"🔧 [{name}]", input_summary)
        elif bt == "tool_result":
            # Some streams place tool_result under assistant — handle defensively.
            emit_block("← [tool_result]", summarize(block.get("content", "")))


def handle_user(ev: dict) -> None:
    """User-side events typically carry tool_results back to the model."""
    msg = ev.get("message", {})
    for block in msg.get("content") or []:
        if block.get("type") == "tool_result":
            content = block.get("content", "")
            if isinstance(content, list):
                # content may be a list of {"type": "text", "text": "..."} blocks
                content = " ".join(
                    c.get("text", json.dumps(c, ensure_ascii=False)) if isinstance(c, dict) else str(c)
                    for c in content
                )
            emit_block("← [tool_result]", summarize(content))


def handle_result(ev: dict) -> None:
    """Final 'result' event — emit a tidy summary."""
    cost = ev.get("total_cost_usd")
    duration_ms = ev.get("duration_ms")
    parts = []
    if cost is not None:
        parts.append(f"cost=${cost}")
    if duration_ms is not None:
        parts.append(f"duration={duration_ms}ms")
    if ev.get("num_turns") is not None:
        parts.append(f"turns={ev['num_turns']}")
    sys.stdout.write("\n\n=== DONE === " + "  ".join(parts) + "\n")
    sys.stdout.flush()


def handle_system(ev: dict) -> None:
    """Mostly noise (hook lifecycle). Show only the lightest signal."""
    sub = ev.get("subtype", "")
    # Suppress hook_started / hook_response / init — they're verbose plumbing.
    suppressed = {"hook_started", "hook_response", "init"}
    if sub in suppressed:
        return
    # Anything else: surface a one-liner so we don't completely hide system events.
    emit_block(f"⚙ [system:{sub or '?'}]")


HANDLERS = {
    "assistant": handle_assistant,
    "user": handle_user,
    "result": handle_result,
    "system": handle_system,
}


def main() -> int:
    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if not line.strip():
            continue
        try:
            ev = json.loads(line)
        except json.JSONDecodeError:
            # Pre-stream banner / non-JSON noise — pass through.
            sys.stdout.write(line + "\n")
            sys.stdout.flush()
            continue
        et = ev.get("type")
        h = HANDLERS.get(et)
        if h is not None:
            try:
                h(ev)
            except Exception as exc:
                emit_block(f"⚠ [pretty-stream error in handler {et}]", str(exc))
        # Unknown event types: silently skip.
    return 0


if __name__ == "__main__":
    sys.exit(main())
