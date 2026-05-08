#!/usr/bin/env bash
# skills/imgui-cpp-development/scripts/check-imgui-version.sh
#
# Tiny helper: given a path to imgui.h, emits the version + branch heuristic.
# Used as a building block by the slash commands and hooks.
#
# Usage:
#   check-imgui-version.sh <path-to-imgui.h>
#
# Output (one line, machine-friendly):
#   IMGUI_VERSION=1.92.7 IMGUI_VERSION_NUM=19270 IMGUI_HAS_DOCK=1
#
# Exit codes:
#   0  parsed successfully
#   1  file missing or not an imgui.h

set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'usage: %s <path-to-imgui.h>\n' "$(basename "$0")" >&2
  exit 1
fi

hdr="$1"

if [[ ! -f "${hdr}" ]]; then
  printf 'no such file: %s\n' "${hdr}" >&2
  exit 1
fi

if ! grep -q '^#define IMGUI_VERSION ' "${hdr}" 2>/dev/null; then
  printf 'not an imgui.h (missing IMGUI_VERSION macro): %s\n' "${hdr}" >&2
  exit 1
fi

ver="$(awk -F'"' '/^#define IMGUI_VERSION /{print $2; exit}' "${hdr}")"
vernum="$(awk '/^#define IMGUI_VERSION_NUM /{print $3; exit}' "${hdr}")"
has_dock=0
grep -qE '^#define IMGUI_HAS_DOCK' "${hdr}" 2>/dev/null && has_dock=1

printf 'IMGUI_VERSION=%s IMGUI_VERSION_NUM=%s IMGUI_HAS_DOCK=%s\n' "${ver}" "${vernum}" "${has_dock}"
