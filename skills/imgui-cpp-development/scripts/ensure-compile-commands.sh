#!/usr/bin/env bash
# skills/imgui-cpp-development/scripts/ensure-compile-commands.sh
#
# Verifies that compile_commands.json exists for the project, which clangd
# (and therefore the LSP tool) needs in order to navigate Dear ImGui's
# monofiles. If absent, prints a project-appropriate snippet that enables it
# and the command to regenerate it.
#
# This script does NOT modify the user's project — it only inspects and
# advises. Modifying CMakeLists.txt / meson.build is left to the model so
# the user can review the diff.
#
# Usage:
#   ensure-compile-commands.sh [project-root]
#
# Exit codes:
#   0  compile_commands.json found OR snippet successfully advised
#   1  invalid args / unexpected error

set -euo pipefail

ROOT="${1:-$PWD}"
if [[ ! -d "${ROOT}" ]]; then
  printf 'project root does not exist: %s\n' "${ROOT}" >&2
  exit 1
fi
ROOT="$(cd "${ROOT}" && pwd)"

# --- already present? ---
for cc in "${ROOT}/compile_commands.json" "${ROOT}/build/compile_commands.json" "${ROOT}/build-debug/compile_commands.json"; do
  if [[ -f "${cc}" ]]; then
    printf '\033[32m✓\033[0m compile_commands.json present: %s\n' "${cc}"
    # Soft check: clangd often wants the file at the project root, even if it's also in build/.
    # Suggest a symlink if only the build copy exists.
    if [[ "${cc}" != "${ROOT}/compile_commands.json" && ! -e "${ROOT}/compile_commands.json" ]]; then
      printf '  hint: clangd usually picks up a symlink at the project root.\n'
      printf '        ln -sf %s %s/compile_commands.json\n' "${cc#${ROOT}/}" "${ROOT}"
    fi
    exit 0
  fi
done

printf '\033[33m!\033[0m compile_commands.json not found under %s\n' "${ROOT}"
printf '\nclangd (and the LSP tool) need this to resolve symbols inside ImGui.\n'

# --- detect build system, suggest a project-appropriate snippet ---
if [[ -f "${ROOT}/CMakeLists.txt" ]]; then
  printf '\nDetected CMake project. Add to CMakeLists.txt (top of file):\n\n'
  printf '    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n\n'
  printf 'Then re-configure:\n\n'
  printf '    cmake -S . -B build\n'
  printf '    ln -sf build/compile_commands.json compile_commands.json   # so clangd sees it\n'
elif [[ -f "${ROOT}/meson.build" ]]; then
  printf '\nDetected Meson project. Meson always emits compile_commands.json into the build dir.\n'
  printf 'Run:\n\n'
  printf '    meson setup build\n'
  printf '    ln -sf build/compile_commands.json compile_commands.json\n'
elif [[ -f "${ROOT}/BUILD" || -f "${ROOT}/BUILD.bazel" || -f "${ROOT}/MODULE.bazel" || -f "${ROOT}/WORKSPACE" ]]; then
  printf '\nDetected Bazel project. Use a third-party extractor:\n\n'
  printf '    https://github.com/hedronvision/bazel-compile-commands-extractor\n\n'
  printf 'Add to MODULE.bazel + run `bazel run @hedron_compile_commands//:refresh_all`.\n'
elif [[ -f "${ROOT}/premake5.lua" ]]; then
  printf '\nDetected Premake project. Premake5 itself does not emit compile_commands.json.\n'
  printf 'Workaround: generate Makefiles, then use bear:\n\n'
  printf '    premake5 gmake2\n'
  printf '    bear -- make config=debug\n'
elif [[ -f "${ROOT}/Makefile" || -f "${ROOT}/makefile" ]]; then
  printf '\nDetected Makefile project. Wrap your build with bear:\n\n'
  printf '    bear -- make\n\n'
  printf '(install bear via your package manager: `apt install bear`, `brew install bear`, etc.)\n'
else
  printf '\nNo recognized build system at the project root. General options:\n'
  printf '  • If you build with CMake elsewhere: add `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` and symlink result.\n'
  printf '  • If you build with raw compiler invocations: wrap with bear (`bear -- <build cmd>`).\n'
fi

exit 0
