#!/usr/bin/env bash
# Heuristic Begin/End and Push/Pop pairing check for ImGui code.
#
# Self-gates:
#   * exits 0 silently if IMGUI_LINT_DISABLE=1
#   * exits 0 silently if no imgui.h is reachable from the file's project root
#   * exits 0 silently if the file has more than 20k lines (probably upstream / generated)
#   * exits 0 silently if the file contains a `// imgui-lint: allow imbalance` marker
#
# Non-blocking: always exits 0. Findings go to stderr.
#
# Caveats: per-file Begin/End counts can produce false positives if a single
# ImGui frame's submission spans multiple files. Suppress with the marker above.

set -uo pipefail
set +e   # heuristic script — single grep failure shouldn't abort the whole run

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

# Walk up looking for imgui.h or .git as the project boundary.
ROOT="$(dirname "${FILE}")"
while [ "${ROOT}" != "/" ]; do
  for cand in imgui.h third_party/imgui/imgui.h external/imgui/imgui.h vendor/imgui/imgui.h imgui/imgui.h; do
    if [ -f "${ROOT}/${cand}" ]; then
      break 2
    fi
  done
  if [ -d "${ROOT}/.git" ]; then break; fi
  ROOT="$(dirname "${ROOT}")"
done

HAS=0
for cand in imgui.h third_party/imgui/imgui.h external/imgui/imgui.h vendor/imgui/imgui.h imgui/imgui.h; do
  if [ -f "${ROOT}/${cand}" ]; then HAS=1; break; fi
done

if [ "${HAS}" -eq 0 ]; then
  grep -qE 'ImGui::' "${FILE}" 2>/dev/null || exit 0
fi

if grep -qE '//[[:space:]]*imgui-lint:[[:space:]]*allow[[:space:]]+imbalance' "${FILE}" 2>/dev/null; then
  exit 0
fi

PAIRS=(
  'Begin\b|End\b|window'
  'BeginChild|EndChild|child window'
  'BeginGroup|EndGroup|group'
  'BeginDisabled|EndDisabled|disabled block'
  'BeginMenu\b|EndMenu|menu'
  'BeginMainMenuBar|EndMainMenuBar|main menu bar'
  'BeginMenuBar|EndMenuBar|menu bar'
  'BeginCombo|EndCombo|combo'
  'BeginListBox|EndListBox|list box'
  'BeginTooltip|EndTooltip|tooltip'
  'BeginItemTooltip|EndTooltip|item tooltip (shares EndTooltip)'
  'BeginTable|EndTable|table'
  'BeginTabBar|EndTabBar|tab bar'
  'BeginTabItem|EndTabItem|tab item'
  'BeginDragDropSource|EndDragDropSource|drag-drop source'
  'BeginDragDropTarget|EndDragDropTarget|drag-drop target'
  'PushID|PopID|ID stack'
  'PushFont|PopFont|font'
  'PushStyleColor|PopStyleColor|style color'
  'PushStyleVar|PopStyleVar|style var'
  'PushItemFlag|PopItemFlag|item flag'
  'PushItemWidth|PopItemWidth|item width'
  'PushTextWrapPos|PopTextWrapPos|text wrap pos'
  'PushClipRect|PopClipRect|clip rect'
)

PB=$(grep -cE 'ImGui::(BeginPopup\b|BeginPopupModal|BeginPopupContextItem|BeginPopupContextWindow|BeginPopupContextVoid)[[:space:]]*\(' "${FILE}" 2>/dev/null | head -1)
PE=$(grep -cE 'ImGui::EndPopup[[:space:]]*\(' "${FILE}" 2>/dev/null | head -1)
PB="${PB:-0}"
PE="${PE:-0}"

WARNED=0
print_header() {
  if [ "${WARNED}" -eq 0 ]; then
    printf '\n[imgui-pair] %s\n' "${FILE}" >&2
    WARNED=1
  fi
}
warn() { print_header; printf '  ! %s\n' "$*" >&2; }

count_grep() {
  local n
  n=$(grep -cE "ImGui::${1}[[:space:]]*\(" "${FILE}" 2>/dev/null | head -1)
  echo "${n:-0}"
}

for pair in "${PAIRS[@]}"; do
  bp=${pair%%|*}
  rest=${pair#*|}
  ep=${rest%%|*}
  desc=${rest#*|}
  bn=$(count_grep "${bp}")
  en=$(count_grep "${ep}")
  if [ "${bn}" != "${en}" ]; then
    warn "${desc}: ${bn} ${bp} vs ${en} ${ep} -- counts disagree"
  fi
done

if [ "${PB}" != "${PE}" ]; then
  warn "popups: ${PB} BeginPopup* vs ${PE} EndPopup -- counts disagree"
fi

if [ "${WARNED}" -eq 1 ]; then
  printf '\n  Heuristic per-file count. To suppress, add this line in the file:\n  // imgui-lint: allow imbalance\n' >&2
fi

exit 0
