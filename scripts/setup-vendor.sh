#!/usr/bin/env bash
# scripts/setup-vendor.sh
#
# Pulls upstream sources we research against into vendor/. Idempotent: skips
# repos that are already at the expected tag.
#
# Why this exists: the shipped skill never bundles upstream source, but we (its
# developers) write reference docs from the actual upstream files. Dear ImGui's
# monofiles are the canonical documentation by upstream's own description, so
# every reference-doc edit should be grounded in this directory rather than
# training-data memory.
#
# Layout produced:
#   vendor/imgui              ocornut/imgui              tag: v1.92.7-docking
#   vendor/glfw               glfw/glfw                  tag: 3.4
#   vendor/imgui_test_engine  ocornut/imgui_test_engine  tag: v1.92  (latest stable)
#   vendor/notes/             scratch dir for our research notes (created empty)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENDOR_DIR="${REPO_ROOT}/vendor"

# Pinned upstream sources.
# Format: <subdir> <git-url> <ref> <description>
PINS=(
  "imgui|https://github.com/ocornut/imgui.git|v1.92.7-docking|Dear ImGui (docking branch tag — primary research target)"
  "glfw|https://github.com/glfw/glfw.git|3.4|GLFW (windowing/input library used by the OpenGL3+GLFW backend)"
  "imgui_test_engine|https://github.com/ocornut/imgui_test_engine.git|v1.92.7|imgui_test_engine (headless testing for ImGui apps; pinned to match imgui)"
)

mkdir -p "${VENDOR_DIR}/notes/issues"

ok()    { printf '\033[32m✓\033[0m %s\n' "$*"; }
info()  { printf '\033[36m·\033[0m %s\n' "$*"; }
warn()  { printf '\033[33m!\033[0m %s\n' "$*" >&2; }
fail()  { printf '\033[31m✗\033[0m %s\n' "$*" >&2; }

clone_or_update() {
  local subdir="$1" url="$2" ref="$3" desc="$4"
  local target="${VENDOR_DIR}/${subdir}"

  info "${subdir}: ${desc}"

  if [[ -d "${target}/.git" ]]; then
    local current
    current="$(git -C "${target}" describe --tags --exact-match 2>/dev/null \
            || git -C "${target}" rev-parse --short HEAD)"
    if [[ "${current}" == "${ref}" ]]; then
      ok "${subdir}: already at ${ref}"
      return 0
    fi
    info "${subdir}: updating from ${current} → ${ref}"
    git -C "${target}" fetch --tags --depth=1 origin "${ref}" \
      || { fail "${subdir}: fetch failed"; return 1; }
    git -C "${target}" checkout --detach "${ref}" \
      || { fail "${subdir}: checkout ${ref} failed"; return 1; }
    ok "${subdir}: checked out ${ref}"
  else
    info "${subdir}: cloning ${url} @ ${ref}"
    if ! git clone --depth=1 --branch "${ref}" "${url}" "${target}" 2>/dev/null; then
      # Branch/tag form failed (some refs don't work with --branch on shallow clone).
      # Fall back to full clone + checkout.
      warn "${subdir}: shallow-branch clone failed, retrying with full clone"
      git clone "${url}" "${target}" \
        || { fail "${subdir}: clone failed"; return 1; }
      git -C "${target}" checkout --detach "${ref}" \
        || { fail "${subdir}: checkout ${ref} failed"; return 1; }
    fi
    ok "${subdir}: cloned at ${ref}"
  fi
}

main() {
  echo "Setting up vendor/ at ${VENDOR_DIR}"
  echo

  local rc=0
  for pin in "${PINS[@]}"; do
    IFS='|' read -r subdir url ref desc <<<"${pin}"
    if ! clone_or_update "${subdir}" "${url}" "${ref}" "${desc}"; then
      rc=1
    fi
  done

  echo
  if [[ ${rc} -eq 0 ]]; then
    ok "vendor/ ready"
    echo
    echo "Next steps:"
    echo "  1. Generate compile_commands.json for vendor/imgui to enable LSP-driven navigation"
    echo "     (see skills/imgui-cpp-development/references/lsp-navigation.md once written)"
    echo "  2. Read skills/imgui-cpp-development/references/<topic>.md against vendor/imgui as ground truth"
  else
    fail "vendor/ setup completed with errors — fix the messages above and re-run"
  fi
  return ${rc}
}

main "$@"
