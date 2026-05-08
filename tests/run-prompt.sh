#!/usr/bin/env bash
# tests/run-prompt.sh
#
# Run a prompt under prompts/ as a non-interactive Claude Code session,
# streaming output live to your terminal AND saving a transcript locally.
#
# Defaults bake in test isolation:
#   - CLAUDE_CODE_SKIP_PROMPT_HISTORY=1 keeps the run out of
#     ~/.claude/history.jsonl and the per-project transcript dir.
#   - Transcript saved to transcripts/<NN>-<slug>__<UTC>.txt (gitignored).
#
# Usage:
#   ./run-prompt.sh <NN-slug>            # text output (default)
#   ./run-prompt.sh <NN-slug> --json     # full stream-json (tool-use detail)
#   ./run-prompt.sh 04-debug-delete-button
#
# Examples:
#   ./run-prompt.sh 04-debug-delete-button
#   ./run-prompt.sh 04-debug-delete-button --json | tail -f
#
# Notes:
#   - Run from inside `tests/`. The session inherits this cwd, so it picks up
#     tests/CLAUDE.md and writes its <NN>-<slug>/ output dir there.
#   - Text mode shows the model's prose response as it streams; --json mode
#     shows every tool use, partial message, and result event for grading.

set -euo pipefail

usage() {
  sed -n '1,/^$/ {/^# /p}' "${BASH_SOURCE[0]}" | sed 's/^# \?//'
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

PROMPT_NAME="$1"
shift

FORMAT="text"
EXT="txt"
if [[ "${1:-}" == "--json" ]]; then
  FORMAT="stream-json"
  EXT="jsonl"
  shift
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROMPT_FILE="${SCRIPT_DIR}/prompts/${PROMPT_NAME}.md"

if [[ ! -f "${PROMPT_FILE}" ]]; then
  printf 'no such prompt: %s\n' "${PROMPT_FILE}" >&2
  printf 'available:\n' >&2
  ls "${SCRIPT_DIR}/prompts/" | grep '\.md$' | sed 's/\.md$//' | sed 's/^/  /' >&2
  exit 1
fi

cd "${SCRIPT_DIR}"
mkdir -p transcripts
ts="$(date -u +%Y%m%dT%H%M%SZ)"
out="transcripts/${PROMPT_NAME}__${ts}.${EXT}"

# Echo the prompt up front so the transcript is self-contained and you can
# see exactly what was sent — both in the file and on your terminal.
{
  printf '=== PROMPT (%s) ===\n' "${PROMPT_NAME}"
  cat "${PROMPT_FILE}"
  printf '\n=== CLAUDE (%s, %s, model auto-selected) ===\n' "${FORMAT}" "${ts}"
} | tee "${out}"

export CLAUDE_CODE_SKIP_PROMPT_HISTORY=1

# Two streaming gotchas this guards against:
#   1. Pipe buffering — when claude -p's stdout is a pipe (we're teeing),
#      libc switches to full-buffer mode, so tee sees nothing until claude
#      finishes and flushes. stdbuf -oL forces line-buffering so each line
#      arrives at tee as it's produced.
#   2. Thinking mode silence — with CLAUDE_EFFORT=xhigh (or any effort > 0),
#      the model can think for 30-60+ seconds before any visible text
#      streams. In text mode (default), thinking events aren't shown, so
#      the terminal looks dead. If you want to SEE the model thinking and
#      tool-using as it happens, use --json instead.
printf '· (thinking phase may run 30-60s before any prose streams; --json shows live thinking + tool use events)\n' >&2

if [[ "${FORMAT}" == "stream-json" ]]; then
  stdbuf -oL claude -p "$(cat "${PROMPT_FILE}")" --output-format stream-json 2>&1 \
    | stdbuf -oL tee -a "${out}"
else
  stdbuf -oL claude -p "$(cat "${PROMPT_FILE}")" 2>&1 \
    | stdbuf -oL tee -a "${out}"
fi

printf '\n· transcript saved to %s\n' "${out}" >&2
