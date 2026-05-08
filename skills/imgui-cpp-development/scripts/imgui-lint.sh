#!/usr/bin/env bash
# High-precision regex lint rules for ImGui code.
#
# Rules implemented:
#   - openpopup-every-frame
#   - missing-imgui-checkversion
#   - render-without-newframe
#   - setnextwindow-after-begin
#
# Per-rule opt-out: place `// imgui-lint: allow <rule-name>` on the same line
# as the warning to suppress that single instance.
#
# Self-gates and is non-blocking. Always exits 0; findings go to stderr.

set -euo pipefail

FILE="${1:-}"
[ -z "${FILE}" ] && exit 0
[ ! -f "${FILE}" ] && exit 0
[ "${IMGUI_LINT_DISABLE:-0}" = "1" ] && exit 0

case "${FILE}" in
  *.cpp|*.cc|*.cxx|*.h|*.hpp|*.hxx) ;;
  *) exit 0 ;;
esac

lc=$(wc -l < "${FILE}" 2>/dev/null || echo 0)
[ "${lc}" -gt 20000 ] && exit 0

grep -qE '(ImGui::|IMGUI_)' "${FILE}" 2>/dev/null || exit 0

WARNED=0
print_header() {
  if [ "${WARNED}" -eq 0 ]; then
    printf '\n[imgui-lint] %s\n' "${FILE}" >&2
    WARNED=1
  fi
}

emit() {
  local lineno="$1" rule="$2" msg="$3"
  local line
  line=$(sed -n "${lineno}p" "${FILE}" 2>/dev/null || true)
  if echo "${line}" | grep -qE "//[[:space:]]*imgui-lint:[[:space:]]*allow[[:space:]]+${rule}"; then
    return 0
  fi
  print_header
  printf '  %s:%s  [%s]  %s\n' "${FILE}" "${lineno}" "${rule}" "${msg}" >&2
}

# rule: openpopup-every-frame
# Heuristic: ImGui::OpenPopup at top-level (no `if (...)` on the same or
# previous line). Catches the most common popup bug: re-arming each frame.
while IFS=: read -r ln content; do
  prev_line=$(sed -n "$((ln - 1))p" "${FILE}" 2>/dev/null || true)
  if ! echo "${content}" | grep -qE 'if[[:space:]]*\('; then
    if ! echo "${prev_line}" | grep -qE 'if[[:space:]]*\('; then
      emit "${ln}" 'openpopup-every-frame' \
        "OpenPopup at top-level - popups need a one-shot trigger (e.g. inside an 'if (button click)'). See modals-and-popups.md."
    fi
  fi
done < <(grep -nE 'ImGui::OpenPopup[[:space:]]*\(' "${FILE}" 2>/dev/null || true)

# rule: missing-imgui-checkversion
if grep -qE 'ImGui::CreateContext[[:space:]]*\(' "${FILE}" 2>/dev/null; then
  if ! grep -qE 'IMGUI_CHECKVERSION[[:space:]]*\(' "${FILE}" 2>/dev/null; then
    line=$(grep -nE 'ImGui::CreateContext[[:space:]]*\(' "${FILE}" | head -1 | cut -d: -f1)
    emit "${line:-1}" 'missing-imgui-checkversion' \
      "CreateContext without IMGUI_CHECKVERSION() - silent ABI mismatch when ImGui is updated. Add IMGUI_CHECKVERSION() before CreateContext."
  fi
fi

# rule: render-without-newframe
if grep -qE 'ImGui::Render[[:space:]]*\(' "${FILE}" 2>/dev/null; then
  if ! grep -qE 'ImGui::NewFrame[[:space:]]*\(' "${FILE}" 2>/dev/null; then
    line=$(grep -nE 'ImGui::Render[[:space:]]*\(' "${FILE}" | head -1 | cut -d: -f1)
    emit "${line:-1}" 'render-without-newframe' \
      "Render() in a file with no NewFrame() - heuristic only; suppress if your main loop is split across files."
  fi
fi

# rule: setnextwindow-after-begin
# Heuristic: for each `ImGui::Begin(`, look at the next 30 lines and flag
# any SetNextWindow* call that appears before the matching End or close-brace.
while IFS=: read -r begin_line _content; do
  end_line=$((begin_line + 30))
  found=$(awk -v s="$((begin_line + 1))" -v e="${end_line}" '
    NR >= s && NR <= e {
      if (/ImGui::End[[:space:]]*\([[:space:]]*\)/) exit
      if (/^}/) exit
      if (/ImGui::SetNextWindow(Size|Pos|SizeConstraints|FocusOnAppearing|Collapsed|Bg)/) {
        print NR
        exit
      }
    }
  ' "${FILE}")
  if [ -n "${found}" ]; then
    emit "${found}" 'setnextwindow-after-begin' \
      "SetNextWindow* AFTER Begin() in the same scope - must be called BEFORE Begin(). See layout-and-sizing.md"
  fi
done < <(grep -nE 'ImGui::Begin[[:space:]]*\(' "${FILE}" 2>/dev/null || true)

if [ "${WARNED}" -eq 1 ]; then
  printf '\n  Suppress a single line: add `// imgui-lint: allow <rule>` at end of line.\n' >&2
  printf   '  Suppress whole file:    export IMGUI_LINT_DISABLE=1\n' >&2
fi

exit 0
