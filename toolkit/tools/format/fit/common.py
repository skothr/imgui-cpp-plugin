#!/usr/bin/env python3
"""Shared helpers for the fit scripts: corpus selection, clang-format invocation, line metrics.

The corpus is only read.  Every script takes the corpus directory as an argument; nothing here
names a location on a particular machine.
"""

import argparse
import difflib
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

# The scripts import each other by their path from the repository root, so that the imports
# resolve the same way for Python and for a type checker run from the root.
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from toolkit.tools.format import imtool_format  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL_DIR = os.path.dirname(HERE)
DEFAULT_STYLE = os.path.join(TOOL_DIR, "imtool.clang-format")

EXTS = (".hpp", ".cpp", ".h", ".cu", ".cuh")

# directory names that hold third-party or generated code inside a project
EXCLUDED_DIR_NAMES = {"lib", "libs", "third_party", "imgui", "vendor", "build", "ext", "external", "deps"}
# third-party files that sit next to a project's own sources
EXCLUDED_FILE_RE = re.compile(
    r"^(stb_.*|json\.hpp|imgui.*|imconfig.*|imstb.*|implot.*|glad.*|khrplatform.*|catch\.hpp|tiny.*|vk_mem_alloc.*)$",
    re.I,
)
# generated files (configure_file outputs)
EXCLUDED_GENERATED_RE = re.compile(r"(^|/)version/[^/]+$|(^|/)astrosimConfig\.h$")
# Corpus-relative path prefixes left out by default.  These name directories of the corpus the
# style was fitted on; pass --exclude to replace the list for another corpus.
#   counter-example-gui_cpp, _my-prior-attempts : not the owner's hand style
#   vk-test-premake : 4-space K&R with tabs and CRLF line endings (tutorial-derived)
#   19-logos/old/args, 19-logos/old/argparse : third-party libraries kept under a project
DEFAULT_EXCLUDE = [
    "counter-example-gui_cpp/",
    "_my-prior-attempts/",
    "vk-test-premake/",
    "19-logos/old/args/",
    "19-logos/old/argparse/",
]


def add_corpus_arguments(ap):
    ap.add_argument("--corpus", required=True, help="directory with one sub-directory per project of hand-formatted C++")
    ap.add_argument(
        "--exclude",
        action="append",
        default=None,
        help="corpus-relative path prefix to leave out (repeatable; default: the list in common.DEFAULT_EXCLUDE)",
    )


def corpus_files(args):
    """(absolute corpus root, relative paths of the selected source files)."""
    root = os.path.abspath(args.corpus)
    if not os.path.isdir(root):
        raise SystemExit(f"corpus directory not found: {args.corpus}")
    excludes = DEFAULT_EXCLUDE if args.exclude is None else args.exclude
    files = list_corpus(root, excludes)
    if not files:
        raise SystemExit(f"no C++ sources found under {args.corpus}")
    return root, files


def list_corpus(root, excludes=()):
    """Relative paths (project/...) of the hand-written sources under root."""
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if d not in EXCLUDED_DIR_NAMES and not d.startswith("."))
        for fn in sorted(filenames):
            if not fn.endswith(EXTS):
                continue
            rel = os.path.relpath(os.path.join(dirpath, fn), root).replace(os.sep, "/")
            if "/" not in rel:
                continue  # a file directly in the corpus root belongs to no project
            if any(rel.startswith(p) for p in excludes):
                continue
            if EXCLUDED_FILE_RE.match(fn) or EXCLUDED_GENERATED_RE.search(rel):
                continue
            out.append(rel)
    return out


def list_files(root):
    """Relative paths of all C++ sources under root (no exclusions)."""
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for fn in sorted(filenames):
            if fn.endswith(EXTS):
                out.append(os.path.relpath(os.path.join(dirpath, fn), root).replace(os.sep, "/"))
    return out


def resolve_binary(explicit):
    """The clang-format binary to use: the argument, else the formatter's own lookup."""
    binary = imtool_format.find_binary(explicit)
    if not binary:
        raise SystemExit("no clang-format binary found; pass --clang-format BIN")
    return binary


def binary_version(binary):
    p = subprocess.run([binary, "--version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    m = re.search(r"version (\d+(?:\.\d+)*)", p.stdout.decode("utf-8", "replace"))
    if m is None:
        raise SystemExit(f"{binary} does not report a clang-format version")
    return m.group(1)


def read(path):
    with open(path, "r", encoding="utf-8", errors="surrogateescape", newline="") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="utf-8", errors="surrogateescape", newline="") as f:
        f.write(text)


def clang_format(binary, style_file, text, assume="x.cpp"):
    """Format text with clang-format; .cu, .cuh and .h are formatted as C++."""
    p = subprocess.run(
        [binary, "--style=file:" + style_file, "--assume-filename=" + assume],
        input=text.encode("utf-8", "surrogateescape"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if p.returncode != 0:
        raise RuntimeError("clang-format failed: " + p.stderr.decode("utf-8", "replace")[:500])
    return p.stdout.decode("utf-8", "surrogateescape")


_WS = re.compile(r"\s+")


def normalize(text, mode):
    """mode: 'exact' | 'noindent' (leading whitespace of each line dropped) | 'nows' (all whitespace dropped per line)."""
    if mode == "exact":
        return text
    lines = text.split("\n")
    if mode == "noindent":
        lines = [line.lstrip(" \t") for line in lines]
    elif mode == "nows":
        lines = [_WS.sub("", line) for line in lines]
    else:
        raise ValueError(mode)
    return "\n".join(lines)


def count_lines(text):
    n = text.count("\n")
    if text and not text.endswith("\n"):
        n += 1
    return n


def changed_lines(a, b, mode="exact"):
    """Number of lines of a that a line diff against b removes or changes."""
    if mode != "exact":
        a, b = normalize(a, mode), normalize(b, mode)
    if a == b:
        return 0
    names = []
    try:
        for text in (a, b):
            f = tempfile.NamedTemporaryFile("w", delete=False, encoding="utf-8", errors="surrogateescape", newline="")
            names.append(f.name)
            f.write(text)
            f.close()
        p = subprocess.run(["diff", "--text", names[0], names[1]], stdout=subprocess.PIPE)
        return sum(1 for line in p.stdout.split(b"\n") if line.startswith(b"<"))
    finally:
        for name in names:
            os.unlink(name)


def removed_lines(a, b):
    """Diff hunks between two texts: [(old lines, new lines, index of the first old line)]."""
    al, bl = a.split("\n"), b.split("\n")
    sm = difflib.SequenceMatcher(None, al, bl, autojunk=False)
    hunks = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag != "equal":
            hunks.append((al[i1:i2], bl[j1:j2], i1))
    return hunks


def pmap(fn, items, workers=None):
    workers = workers or (os.cpu_count() or 4)
    with ThreadPoolExecutor(max_workers=workers) as ex:
        return list(ex.map(fn, items))


def project_of(rel):
    return rel.split("/", 1)[0]


def strip_all_ws(text):
    return _WS.sub("", text)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="List the corpus files the fit scripts would use, with line counts per project.")
    add_corpus_arguments(ap)
    root, files = corpus_files(ap.parse_args())
    per = {}
    for rel in files:
        d = per.setdefault(project_of(rel), [0, 0])
        d[0] += 1
        d[1] += count_lines(read(os.path.join(root, rel)))
    for name, (n, lines) in sorted(per.items()):
        print(f"{name:24s} files {n:4d} lines {lines:7d}")
    print(f"{'TOTAL':24s} files {len(files):4d} lines {sum(v[1] for v in per.values()):7d}")
