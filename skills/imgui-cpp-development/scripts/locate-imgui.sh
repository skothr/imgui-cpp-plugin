#!/usr/bin/env bash
# skills/imgui-cpp-development/scripts/locate-imgui.sh
#
# Scans a project tree for Dear ImGui copies, identifies version + branch,
# and reports findings. Used as the skill's first action so subsequent guidance
# is grounded in the version actually present.
#
# Usage:
#   locate-imgui.sh [--json] [--quiet] [project-root]
#
#   project-root  Defaults to $PWD.
#   --json        Emit a JSON object instead of human-readable text.
#   --quiet       Suppress informational logs (errors still go to stderr).
#
# Exit codes:
#   0  one or more imgui.h copies found
#   2  no imgui.h found in any common location
#   1  invalid arguments / unexpected error

set -euo pipefail

usage() {
  sed -n '1,/^$/ {/^# /p}' "${BASH_SOURCE[0]}" | sed 's/^# \?//'
}

# --- arg parsing ---
JSON=0
QUIET=0
ROOT=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --json)   JSON=1; shift ;;
    --quiet)  QUIET=1; shift ;;
    -h|--help) usage; exit 0 ;;
    --) shift; break ;;
    -*) printf 'unknown flag: %s\n' "$1" >&2; exit 1 ;;
    *)  ROOT="$1"; shift ;;
  esac
done
ROOT="${ROOT:-$PWD}"

if [[ ! -d "${ROOT}" ]]; then
  printf 'project root does not exist: %s\n' "${ROOT}" >&2
  exit 1
fi
ROOT="$(cd "${ROOT}" && pwd)"

log() { [[ ${QUIET} -eq 1 ]] || printf '· %s\n' "$*" >&2; }

# --- search roots, ordered by likelihood ---
# Each entry is a path relative to ${ROOT}. The presence of imgui.h there is checked.
# We also do a bounded find(1) sweep with -maxdepth 6 to catch FetchContent-style nested dirs.
declare -a CANDIDATE_DIRS=(
  "."
  "imgui"
  "third_party/imgui" "third_party/Dear_Imgui" "third_party/dear_imgui"
  "external/imgui"
  "vendor/imgui"
  "deps/imgui"
  "lib/imgui" "libs/imgui"
  "src/imgui" "src/external/imgui" "src/third_party/imgui"
  "ext/imgui"
  "subprojects/imgui"
  "submodules/imgui"
  "build/_deps/imgui-src"
  "build/_deps/dear_imgui-src"
  "out/build/_deps/imgui-src"
  "_deps/imgui-src"
)

# --- collect findings (deduplicated by realpath) ---
declare -a FOUND_FILES=()
declare -A SEEN

add_if_imgui_h() {
  local path="$1"
  [[ -f "${path}" ]] || return 1
  # Quick sanity check: file must contain IMGUI_VERSION macro to count.
  grep -q '^#define IMGUI_VERSION' "${path}" 2>/dev/null || return 1
  local rp
  rp="$(realpath "${path}")"
  if [[ -z "${SEEN[${rp}]:-}" ]]; then
    SEEN[${rp}]=1
    FOUND_FILES+=("${rp}")
  fi
  return 0
}

log "scanning project root: ${ROOT}"

for d in "${CANDIDATE_DIRS[@]}"; do
  add_if_imgui_h "${ROOT}/${d}/imgui.h" || true
done

# Bounded find for anything we missed (FetchContent dirs vary; vcpkg/conan caches).
# -maxdepth 6 keeps this fast on large monorepos.
while IFS= read -r -d '' candidate; do
  add_if_imgui_h "${candidate}" || true
done < <(find "${ROOT}" -maxdepth 6 -type f -name 'imgui.h' \
           -not -path '*/node_modules/*' \
           -not -path '*/.git/*' \
           -print0 2>/dev/null || true)

# --- parse each finding ---
parse_imgui_h() {
  local hdr="$1"
  local version="" version_num="" docking="false"

  version="$(awk -F'"' '/^#define IMGUI_VERSION /{print $2; exit}' "${hdr}" 2>/dev/null || true)"
  version_num="$(awk '/^#define IMGUI_VERSION_NUM /{print $3; exit}' "${hdr}" 2>/dev/null || true)"

  # The docking branch defines IMGUI_HAS_DOCK as 1; the master branch does not define it.
  if grep -qE '^#define IMGUI_HAS_DOCK' "${hdr}" 2>/dev/null; then
    docking="true"
  fi

  printf '%s|%s|%s' "${version}" "${version_num}" "${docking}"
}

# --- compile_commands.json check (gates LSP navigation accuracy) ---
HAS_COMPILE_COMMANDS=false
COMPILE_COMMANDS_PATH=""
for ccpath in "${ROOT}/compile_commands.json" "${ROOT}/build/compile_commands.json" "${ROOT}/build-debug/compile_commands.json"; do
  if [[ -f "${ccpath}" ]]; then
    HAS_COMPILE_COMMANDS=true
    COMPILE_COMMANDS_PATH="${ccpath}"
    break
  fi
done

# --- output ---
if [[ ${JSON} -eq 1 ]]; then
  printf '{\n'
  printf '  "project_root": "%s",\n' "${ROOT}"
  printf '  "compile_commands": {\n'
  printf '    "found": %s,\n' "${HAS_COMPILE_COMMANDS}"
  printf '    "path": "%s"\n' "${COMPILE_COMMANDS_PATH}"
  printf '  },\n'
  printf '  "imgui_copies": [\n'
  local n=${#FOUND_FILES[@]}
  for ((i=0; i<n; i++)); do
    local f="${FOUND_FILES[i]}"
    IFS='|' read -r ver vernum dock <<<"$(parse_imgui_h "${f}")"
    printf '    {\n'
    printf '      "imgui_h": "%s",\n' "${f}"
    printf '      "version": "%s",\n' "${ver}"
    printf '      "version_num": %s,\n' "${vernum:-0}"
    printf '      "docking_branch": %s\n' "${dock}"
    printf '    }%s\n' "$([[ $((i+1)) -lt n ]] && echo ',' || echo '')"
  done
  printf '  ]\n}\n'
else
  if [[ ${#FOUND_FILES[@]} -eq 0 ]]; then
    printf '\n\033[33m!\033[0m no Dear ImGui copy found under %s\n' "${ROOT}"
    printf '\nNext steps:\n'
    printf '  • Add via CMake FetchContent (target tag v1.92.7-docking):\n'
    printf '      include(FetchContent)\n'
    printf '      FetchContent_Declare(imgui\n'
    printf '          GIT_REPOSITORY https://github.com/ocornut/imgui.git\n'
    printf '          GIT_TAG        v1.92.7-docking)\n'
    printf '      FetchContent_MakeAvailable(imgui)\n'
    printf '  • Or as a git submodule:  git submodule add https://github.com/ocornut/imgui third_party/imgui && git -C third_party/imgui checkout v1.92.7-docking\n'
    printf '  • Or download the tag tarball manually\n'
    exit 2
  fi

  printf '\n\033[32m✓\033[0m found %d Dear ImGui cop%s under %s:\n' \
    "${#FOUND_FILES[@]}" "$([[ ${#FOUND_FILES[@]} -eq 1 ]] && echo y || echo ies)" "${ROOT}"
  for f in "${FOUND_FILES[@]}"; do
    IFS='|' read -r ver vernum dock <<<"$(parse_imgui_h "${f}")"
    printf '\n  %s\n' "${f}"
    printf '    version       : %s (NUM=%s)\n' "${ver:-?}" "${vernum:-?}"
    printf '    branch        : %s\n' "$([[ ${dock} == true ]] && echo 'docking' || echo 'master')"
    if [[ -n "${vernum}" ]] && (( vernum < 19270 )); then
      printf '    \033[33m!\033[0m skill targets v1.92.7 (NUM=19270); this copy is older — major font-system rework happened in v1.92\n'
    fi
  done

  printf '\ncompile_commands.json: %s\n' \
    "$([[ ${HAS_COMPILE_COMMANDS} == true ]] && echo "${COMPILE_COMMANDS_PATH}" || echo 'NOT FOUND — LSP navigation will be limited; see ensure-compile-commands.sh')"
fi
