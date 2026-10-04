#!/usr/bin/env python3
"""Measure the share of corpus lines a formatter configuration changes.

usage: measure.py --corpus DIR [--clang-format BIN] [--style FILE] [--post]

Three shares are printed per project: lines changed, lines changed ignoring leading
indentation, lines changed ignoring all whitespace.  The corpus is formatted in memory.
"""

import argparse
import os
import sys

# The scripts import each other by their path from the repository root, so that the imports
# resolve the same way for Python and for a type checker run from the root.
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from toolkit.tools.format import imtool_format  # noqa: E402
from toolkit.tools.format.fit import common  # noqa: E402


def measure(binary, style, root, files, post=False, modes=("exact", "noindent", "nows")):
    """Returns {project: {mode: changed, 'lines': n}} plus the key 'ALL'."""

    def one(rel):
        text = common.read(os.path.join(root, rel))
        out = common.clang_format(binary, style, text)
        if post:
            out = imtool_format.post_pass(out)
        return rel, common.count_lines(text), {m: common.changed_lines(text, out, m) for m in modes}

    res = {}
    for rel, n, ch in common.pmap(one, files):
        for key in (common.project_of(rel), "ALL"):
            d = res.setdefault(key, {"lines": 0, **{m: 0 for m in modes}})
            d["lines"] += n
            for m in modes:
                d[m] += ch[m]
    return res


def fmt_table(res, modes=("exact", "noindent", "nows")):
    rows = []
    for key in sorted(k for k in res if k != "ALL") + ["ALL"]:
        d = res[key]
        rows.append(f"{key:24s} lines {d['lines']:7d}  " + "  ".join(f"{m} {100.0 * d[m] / max(1, d['lines']):5.1f}%" for m in modes))
    return "\n".join(rows)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    common.add_corpus_arguments(ap)
    ap.add_argument("--clang-format", dest="binary", help="clang-format binary (default: the formatter's own lookup)")
    ap.add_argument("--style", default=common.DEFAULT_STYLE, help="clang-format style file (default: imtool.clang-format)")
    ap.add_argument("--post", action="store_true", help="apply the post-pass of imtool_format.py as well (single round)")
    a = ap.parse_args()
    root, files = common.corpus_files(a)
    print(fmt_table(measure(common.resolve_binary(a.binary), os.path.abspath(a.style), root, files, a.post)))
