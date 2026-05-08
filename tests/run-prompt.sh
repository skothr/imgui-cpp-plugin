#!/usr/bin/env bash
# tests/run-prompt.sh
#
# Run a prompt under prompts/ in a Claude Code session, with test-isolation
# defaults baked in:
#
#   - CLAUDE_CODE_SKIP_PROMPT_HISTORY=1 keeps the prompt out of the global
#     ~/.claude/history.jsonl and the per-project transcript dir.
#   - In non-interactive mode, the full transcript (including tool use) is
#     captured locally to transcripts/<NN>-<slug>__<ISO-time>.jsonl.
#   - In interactive mode, you observe the session live; transcripts can
#     still be saved by passing --capture (uses claude -p in the background
#     after, which doesn't apply here — see notes).
#
# Usage:
#   ./run-prompt.sh <NN-slug>                        # interactive, isolated
#   ./run-prompt.sh <NN-slug> --capture              # non-interactive, archived
#   ./run-prompt.sh 04-debug-delete-button --capture
#
# Notes:
#   - Run from inside `tests/` (this script's directory). The script is a thin
#     wrapper, NOT a substitute for the tests/CLAUDE.md project context — the
#     session still needs to find that file at cwd, which it will.
#   - Interactive mode does NOT auto-archive a transcript because Claude Code's
#     skip-history flag suppresses the per-project transcript too. If you want
#     a recording of an interactive session, run it twice (once interactive to
#     observe, once with --capture to archive).

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
CAPTURE=0
if [[ "${1:-}" == "--capture" ]]; then
  CAPTURE=1
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

# We always run from inside tests/ so the session picks up tests/CLAUDE.md.
cd "${SCRIPT_DIR}"

# Quarantine env: keeps the prompt and the resulting per-project transcript
# out of ~/.claude/.
export CLAUDE_CODE_SKIP_PROMPT_HISTORY=1

if [[ ${CAPTURE} -eq 1 ]]; then
  mkdir -p transcripts
  ts="$(date -u +%Y%m%dT%H%M%SZ)"
  out="transcripts/${PROMPT_NAME}__${ts}.jsonl"
  printf '· running prompt non-interactively, transcript → %s\n' "${out}" >&2
  # stream-json captures every tool use, every model turn — useful for
  # iteration grading later.
  claude -p "$(cat "${PROMPT_FILE}")" --output-format stream-json > "${out}" 2>&1
  printf '· done. transcript saved to %s\n' "${out}" >&2
else
  printf '· starting interactive session in %s\n' "${SCRIPT_DIR}" >&2
  printf '· paste the contents of %s as your first message\n' "${PROMPT_FILE}" >&2
  printf '· (CLAUDE_CODE_SKIP_PROMPT_HISTORY=1 — nothing recorded to ~/.claude)\n' >&2
  claude
fi
