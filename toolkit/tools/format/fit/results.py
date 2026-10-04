#!/usr/bin/env python3
"""Measure the formatter on a corpus (and optionally on other source trees) and write a report.

usage: results.py --corpus DIR [--library DIR ...] [--work DIR] [--clang-format BIN]
                  [--style FILE] [--baseline-config FILE] [--excerpt REL::REGEX ...] > results.txt

The corpus and the libraries are only read: the selected files are copied under --work (a new
temporary directory by default) and imtool_format.py is run on the copies through its command
line, in place, exactly as a user would run it.  The report has: the share of lines changed
(per project and overall; also ignoring indentation and ignoring all whitespace), token identity
and idempotence checks, the files the tool refused, the unbraced-body findings, the effect of
each post-pass feature, and a categorized list of what the tool still changes.
"""

import argparse
import collections
import os
import re
import shutil
import subprocess
import sys
import tempfile

# The scripts import each other by their path from the repository root, so that the imports
# resolve the same way for Python and for a type checker run from the root.
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from toolkit.tools.format import imtool_format  # noqa: E402
from toolkit.tools.format.fit import common, measure  # noqa: E402

TOOL = os.path.join(common.TOOL_DIR, "imtool_format.py")
MODES = ("exact", "noindent", "nows")
MODE_LABEL = {"exact": "lines changed", "noindent": "ignoring leading indentation", "nows": "ignoring all whitespace"}
FINDING = ": control statement without braces: "


def copy_files(src_root, files, dst_root):
    if os.path.isdir(dst_root):
        shutil.rmtree(dst_root)
    for rel in files:
        dst = os.path.join(dst_root, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(os.path.join(src_root, rel), dst)
    return dst_root


def run_tool(root, files, binary, style, check=False):
    """Run the command-line tool over files, in batches: (highest exit status, stdout, stderr)."""
    paths = [os.path.join(root, f) for f in files]

    def one(batch):
        cmd = [sys.executable, TOOL, "--style-file", style, "--clang-format", binary] + (["--check"] if check else []) + batch
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return p.returncode, p.stdout.decode(), p.stderr.decode()

    batches = [paths[i : i + 16] for i in range(0, len(paths), 16)]
    results = common.pmap(one, batches)
    return max(r[0] for r in results), "".join(r[1] for r in results), "".join(r[2] for r in results)


def split_messages(stderr, root):
    """(unbraced-body findings, other messages), with paths made relative to root."""
    findings, other = [], []
    for line in stderr.split("\n"):
        if not line.strip():
            continue
        line = line.replace(root + os.sep, "")
        (findings if FINDING in line else other).append(line)
    return findings, other


def compare_trees(orig_root, new_root, files):
    """Per-project changed-line counts between two trees, plus token checks."""
    res = {}
    stripped_bad = []
    token_bad = []

    def one(rel):
        a = common.read(os.path.join(orig_root, rel))
        b = common.read(os.path.join(new_root, rel))
        ch = {m: common.changed_lines(a, b, m) for m in MODES}
        s_ok = common.strip_all_ws(a) == common.strip_all_ws(b)
        t_ok = imtool_format.token_signature(a) == imtool_format.token_signature(b)
        return rel, common.count_lines(a), ch, s_ok, t_ok, a != b

    n_changed_files = 0
    for rel, n, ch, s_ok, t_ok, differs in common.pmap(one, files):
        n_changed_files += differs
        if not s_ok:
            stripped_bad.append(rel)
        if not t_ok:
            token_bad.append(rel)
        for key in (common.project_of(rel), "ALL"):
            d = res.setdefault(key, {"lines": 0, **{m: 0 for m in MODES}})
            d["lines"] += n
            for m in MODES:
                d[m] += ch[m]
    return res, stripped_bad, token_bad, n_changed_files


def pct(d, m):
    return 100.0 * d[m] / max(1, d["lines"])


def share_table(title, columns, out):
    """columns: [(label, result dict)]; one row per project, one sub-table per mode."""
    out.append(title)
    keys = sorted(k for k in columns[0][1] if k != "ALL") + ["ALL"]
    for m in MODES:
        out.append(f"  -- {MODE_LABEL[m]} (percent of original lines)")
        out.append(
            f"  {'':20s} {'lines':>7s} "
            + " ".join(f"{label:>24s}" for label, _ in columns)
        )
        for k in keys:
            out.append(
                f"  {k:20s} {columns[0][1][k]['lines']:7d} "
                + " ".join(f"{pct(r[k], m):23.1f}%" for _, r in columns)
            )
    out.append("")


# ---------------------------------------------------------------- residual categories

GROUP_NORMAL = "A. normalized on purpose (a stated rule or the majority habit applied to a line that deviates)"
GROUP_NONDET = (
    "B. cannot be formatted deterministically (hand layout that no rule predicts)"
)
GROUP_MORE = "C. could be handled with more work"

_sq = lambda s: re.sub(r" +", " ", s.strip())  # noqa: E731
_nw = lambda s: re.sub(r"\s+", "", s)  # noqa: E731


def spacing_kind(o, n):
    """First place where two lines with equal tokens differ in whitespace -> description."""
    to = [t for t in imtool_format.lex(o.strip())]
    tn = [t for t in imtool_format.lex(n.strip())]
    i = j = 0
    while i < len(to) and j < len(tn):
        a, b = to[i], tn[j]
        if a == b:
            i += 1
            j += 1
            continue
        if a[0] == "ws" and b[0] == "ws":
            i += 1
            j += 1
            continue
        if a[0] == "ws" or b[0] == "ws":
            removed = a[0] == "ws"  # the tool removed a space the owner wrote
            prev = to[i - 1][1] if i > 0 else ""
            nxt = (to[i + 1][1] if i + 1 < len(to) else "") if removed else a[1]
            if prev == ",":
                return (
                    "space after a comma added ('f(a,b)' -> 'f(a, b)')",
                    GROUP_NORMAL,
                )
            if nxt in ("*", "/", "%") or prev in ("*", "/", "%"):
                if removed:
                    return (
                        "spaces around * / % removed ('a * b' -> 'a*b'; survey: 82% unspaced)",
                        GROUP_NORMAL,
                    )
                return ("pointer or * spacing changed", GROUP_NORMAL)
            if nxt in ("+", "-", "==", "!=", "<", ">", "<=", ">=", "&&", "||", "=", "+=", "-=", "*=", "/=", "?", ":", "<<", ">>", "|", "&") or prev in (
                "+", "-", "==", "!=", "<", ">", "<=", ">=", "&&", "||", "=", "+=", "-=", "*=", "/=", "?", ":", "<<", ">>", "|", "&",
            ):  # fmt: skip
                if nxt in ("&",) or prev in ("&",):
                    return (
                        "pointer / reference binding changed ('T& x' -> 'T &x', 'T &f()' -> 'T& f()')",
                        GROUP_NORMAL,
                    )
                return (
                    "spaces around a binary operator added ('a+b' -> 'a + b', 'x=1' -> 'x = 1')",
                    GROUP_NORMAL,
                )
            if prev in ("(", "[") or nxt in (")", "]"):
                return (
                    "space inside parentheses removed ('( x )' -> '(x)')",
                    GROUP_NORMAL,
                )
            if prev == "{" or nxt == "}":
                return (
                    "spaces inside initializer braces added ('{x}' -> '{ x }', rule 6)",
                    GROUP_NORMAL,
                )
            if nxt == "{":
                return (
                    "space before an initializer brace added ('T x{' -> 'T x {')",
                    GROUP_NORMAL,
                )
            if nxt == "(":
                return (
                    "space before a parenthesis removed ('if (' -> 'if(', rule 3)",
                    GROUP_NORMAL,
                )
            if nxt.startswith("//") or nxt.startswith("/*"):
                return ("spacing before a trailing comment", GROUP_NORMAL)
            return ("other spacing inside a line", GROUP_NORMAL)
        break
    return ("other spacing inside a line", GROUP_NORMAL)


def classify_hunk(old, new, limit=140):
    """Yield (category, group) for every old line of a diff hunk."""
    new_exact = set(new)
    new_strip = {n.strip() for n in new}
    new_sq = {_sq(n): n for n in new}
    new_nw = {_nw(n): n for n in new}
    joined = len(new) < len(old)
    for idx, o in enumerate(old):
        s = o.strip()
        if s == "":
            yield ("whitespace-only line emptied, or blank line removed", GROUP_NORMAL)
        elif o.rstrip() in new_exact:
            yield ("trailing whitespace removed", GROUP_NORMAL)
        elif s in new_strip:
            if s.startswith("//"):
                yield (
                    "comment line re-indented to the code level (commented-out code, banner comments)",
                    GROUP_NONDET,
                )
            elif s in ("{", "}", "};", "{ }"):
                yield (
                    "brace line re-indented (block not in GNU layout, or enclosing layout changed)",
                    GROUP_NORMAL,
                )
            else:
                yield (
                    "code line re-indented (hand-aligned continuation, enum body at +3, non-GNU block)",
                    GROUP_NONDET,
                )
        elif _sq(o) in new_sq:
            if re.search(r"\S {2,}//", o):
                yield ("hand alignment: trailing comment column", GROUP_NONDET)
            elif re.search(r" {2,}[-+*/|&]?=[^=]|[^=!<>]= {2,}", o):
                yield ("hand alignment: '=' column (and sign offset)", GROUP_NONDET)
            elif re.search(r", {2,}\S", o) or re.search(r"\( {2,}\S", o):
                yield ("hand alignment: argument / initializer columns", GROUP_NONDET)
            elif re.search(r"\) {2,}\{|\) {2,}(const|override)", o):
                yield (
                    "hand alignment: one-line function bodies lined up in a column",
                    GROUP_NONDET,
                )
            else:
                yield (
                    "hand alignment: declaration names, members, other padding",
                    GROUP_NONDET,
                )
        elif _nw(o) in new_nw:
            yield spacing_kind(o, new_nw[_nw(o)])
        else:
            prev = old[idx - 1].strip() if idx > 0 else ""
            code = re.sub(r"//.*$", "", s).strip()
            body = re.search(r"\{(.*)\}", code)
            n_semi_in = body.group(1).count(";") if body else 0
            outside = re.sub(r"\{.*\}", "", re.sub(r"\(.*\)", "()", code))
            if s.startswith("#"):
                yield (
                    "preprocessor line re-flowed (#define alignment, continuation)",
                    GROUP_NONDET,
                )
            elif s.startswith("//"):
                yield ("comment line moved with re-flowed code", GROUP_NONDET)
            elif len(o) > limit:
                yield ("line over 140 columns wrapped (rule 7)", GROUP_NORMAL)
            elif (
                re.match(r"^\{.*\}\s*;?$", code)
                and joined
                and not re.match(r"^\{\s*\}$", code)
                and prev
                and not prev.endswith((",", "{", "="))
            ):
                yield (
                    "body written alone on its own line, joined onto its statement or signature (it fits)",
                    GROUP_NONDET,
                )
            elif code in ("{", "}", "};") and joined:
                yield (
                    "expanded block of one to three statements collapsed onto one line (it fits)",
                    GROUP_NONDET,
                )
            elif code in ("{", "}", "};", "{ }", "{}"):
                yield ("brace line of a block whose layout changed", GROUP_MORE)
            elif body and n_semi_in >= 2 and not joined:
                yield (
                    "one-line block expanded (four or more statements, or a nested block or comment inside)",
                    GROUP_MORE,
                )
            elif outside.count(";") >= 2 and not code.startswith("for"):
                yield ("several statements on one line split ('a; b;')", GROUP_NONDET)
            elif re.match(r"^(case\b|default\s*:)", code):
                yield ("case label body layout", GROUP_MORE)
            elif re.match(r"^[:,]\s*\w+[({]", code) or re.search(
                r"\)\s*:\s*\w+\(.*\)\s*(\{.*\})?$", code
            ):
                yield ("constructor initializer list re-flowed", GROUP_MORE)
            elif re.match(r"^template\s*<", code):
                yield ("template<> line layout", GROUP_MORE)
            elif (
                re.match(r"^\{.*\}\s*,?\s*\}*;?$", code)
                or re.match(r"^\{", code)
                or re.search(r"=\s*$", prev)
                or re.search(r"=\s*\{?$", code)
            ):
                yield ("braced initializer / table re-flowed", GROUP_NONDET)
            elif body and joined:
                yield ("statement and block joined or re-wrapped", GROUP_NONDET)
            elif body:
                yield (
                    "one-line block re-wrapped (does not fit, or comment inside)",
                    GROUP_MORE,
                )
            elif (
                prev
                and not prev.endswith((";", "{", "}", ":"))
                or not code.endswith((";", "{", "}", ":"))
            ):
                yield (
                    "wrapped expression / argument list re-flowed (hand-chosen break points)",
                    GROUP_NONDET,
                )
            else:
                yield ("other line-break change", GROUP_MORE)


def residual(orig_root, new_root, files, out):
    cats = collections.Counter()
    group_of = {}
    examples = {}
    total = 0
    for rel in files:
        a = common.read(os.path.join(orig_root, rel))
        b = common.read(os.path.join(new_root, rel))
        total += common.count_lines(a)
        if a == b:
            continue
        for old, new, at in common.removed_lines(a, b):
            for (cat, grp), o in zip(classify_hunk(old, new), old):
                cats[cat] += 1
                group_of[cat] = grp
                if (
                    cat not in examples
                    and 1 <= len(old) <= 3
                    and 1 <= len(new) <= 4
                    and o.strip()
                    and max(len(x) for x in old + new) <= 150
                ):
                    examples[cat] = (rel, at + 1, old, new)
    n_changed = sum(cats.values())
    out.append(
        f"changed original lines classified: {n_changed} of {total} ({100.0 * n_changed / total:.1f}%)"
    )
    out.append(
        "(classification uses a longest-common-subsequence line diff; its total can differ from the 'diff' count above by a few lines)"
    )
    for grp in (GROUP_NORMAL, GROUP_NONDET, GROUP_MORE):
        sub = [(c, n) for c, n in cats.most_common() if group_of[c] == grp]
        gsum = sum(n for _, n in sub)
        out.append("")
        out.append(f"{grp}: {gsum} lines, {100.0 * gsum / total:.1f}% of the corpus")
        for c, n in sub:
            out.append(f"  {n:6d}  {100.0 * n / total:4.1f}%  {c}")
            if c in examples:
                rel, at, old, new = examples[c]
                out.append(f"            e.g. {rel}:{at}")
                for l in old:
                    out.append(f"              - {l}")
                for l in new:
                    out.append(f"              + {l}")
    out.append("")


# ---------------------------------------------------------------- excerpts

def excerpt(rel, pattern, count, orig_root, new_root, out):
    """Before / after text of `count` original lines starting at the first line matching pattern."""
    a = common.read(os.path.join(orig_root, rel)).split("\n")
    b = common.read(os.path.join(new_root, rel)).split("\n")
    rx = re.compile(pattern)
    ia = next((i for i, line in enumerate(a) if rx.search(line)), None)
    if ia is None:
        out.append(f"### {rel}: no line matches {pattern}")
        return
    # the same stretch of tokens in the formatted file
    sig_before = len(_nw("\n".join(a[:ia])))
    sig_upto = len(_nw("\n".join(a[: ia + count])))
    seen = 0
    ib = 0
    ie = len(b)
    started = False
    for i, line in enumerate(b):
        if not started and seen >= sig_before and line.strip():
            ib = i
            started = True
        seen += len(_nw(line))
        if seen >= sig_upto:
            ie = i + 1
            break
    out.append(f"### {rel}")
    out.append(f"--- before (lines {ia + 1}..{ia + count})")
    out += ["    " + line for line in a[ia : ia + count]]
    out.append(f"--- after (lines {ib + 1}..{ie})")
    out += ["    " + line for line in b[ib:ie]]
    out.append("")


def run_tree(label, orig_root, files, work, binary, style, out, columns):
    """Format a copy of the tree with the tool, verify it, and append the report lines."""
    fmt_root = copy_files(orig_root, files, work)
    rc1, _so1, se1 = run_tool(fmt_root, files, binary, style)
    final, sbad, tbad, nchanged = compare_trees(orig_root, fmt_root, files)
    findings, other = split_messages(se1, fmt_root)
    refused = sorted({line.split(": ", 2)[1].rstrip(":") for line in other if line.startswith("imtool_format: ")} & set(files))
    share_table("", columns + [("config + post-pass", final)], out)
    kept = [f for f in files if f not in refused]
    out.append(f"{label}: verification (tool run through its command line, in place, on a copy)")
    out.append(f"first run: exit status {rc1}; files rewritten: {nchanged} of {len(files)}; refused and left untouched: {len(refused)}")
    for line in other:
        out.append("    " + line)
    if refused:
        rlines = sum(common.count_lines(common.read(os.path.join(orig_root, f))) for f in refused)
        kres, _, _, _ = compare_trees(orig_root, fmt_root, kept)
        out.append(f"the {len(refused)} refused files ({rlines} lines) count as unchanged in the last column above.")
        out.append(f"over the {len(kept)} files the tool formatted ({kres['ALL']['lines']} lines): " + "; ".join(f"{MODE_LABEL[m]} {pct(kres['ALL'], m):.1f}%" for m in MODES))
    out.append(f"token identity, all whitespace removed, before == after: {len(files) - len(sbad)} of {len(files)} files" + (f"; VIOLATIONS: {sbad}" if sbad else ""))
    out.append(f"token identity, token sequence (identifiers and literals one by one): {len(files) - len(tbad)} of {len(files)} files" + (f"; VIOLATIONS: {tbad}" if tbad else ""))
    snapshot = {f: common.read(os.path.join(fmt_root, f)) for f in files}
    rc_check, so_check, se_check = run_tool(fmt_root, files, binary, style, check=True)
    check_findings, _ = split_messages(se_check, fmt_root)
    run_tool(fmt_root, files, binary, style)
    again = [f for f in files if common.read(os.path.join(fmt_root, f)) != snapshot[f]]
    listed = [line for line in so_check.split("\n") if line.strip()]
    out.append(f"idempotence: --check on the formatted copy lists {len(listed)} files to change (exit status {rc_check}); a second in-place run changed {len(again)} of {len(files)} files" + (f": {again[:20]}" if again else ""))
    per_file = collections.Counter(line.split(":", 1)[0] for line in findings)
    out.append(f"unbraced control-statement bodies reported: {len(findings)} in {len(per_file)} files (rewrite run, as warnings); --check on the formatted copy reports {len(check_findings)} (as errors)")
    for line in findings[:5]:
        out.append("    " + line)
    if len(findings) > 5:
        out.append(f"    ... {len(findings) - 5} more")
    out.append("")
    return fmt_root


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    common.add_corpus_arguments(ap)
    ap.add_argument("--library", action="append", default=[], help="another source tree to format and verify (repeatable)")
    ap.add_argument("--work", help="directory for the copies (default: a new temporary directory)")
    ap.add_argument("--clang-format", dest="binary", help="clang-format binary (default: the formatter's own lookup)")
    ap.add_argument("--style", default=common.DEFAULT_STYLE, help="style file (default: imtool.clang-format)")
    ap.add_argument("--baseline-config", help="another .clang-format to compare against (optional)")
    ap.add_argument("--excerpt", action="append", default=[], help="LIBRARY-RELATIVE-PATH::REGEX of the first line; prints before / after (repeatable)")
    ap.add_argument("--excerpt-lines", type=int, default=16)
    a = ap.parse_args()
    root, cfiles = common.corpus_files(a)
    binary = common.resolve_binary(a.binary)
    style = os.path.abspath(a.style)
    _pinned, limit, width = imtool_format.style_settings(style)
    work = os.path.abspath(a.work) if a.work else tempfile.mkdtemp(prefix="imtool_results_")
    os.makedirs(work, exist_ok=True)
    out = ["# results (generated by fit/results.py)", f"style file: {os.path.basename(style)}; clang-format {common.binary_version(binary)}", ""]

    out.append("# 1. Share of corpus lines changed")
    out.append(f"corpus: {len(cfiles)} files, {sum(common.count_lines(common.read(os.path.join(root, f))) for f in cfiles)} lines")
    columns = []
    if a.baseline_config:
        columns.append(("baseline config", measure.measure(binary, os.path.abspath(a.baseline_config), root, cfiles)))
    columns.append(("config alone", measure.measure(binary, style, root, cfiles)))
    cfmt = run_tree("corpus", root, cfiles, os.path.join(work, "corpus"), binary, style, out, columns)

    out.append("# 2. Post-pass features one at a time (config + only that feature; every file, in memory, one round)")

    def with_features(feats):
        def one(rel):
            text = common.read(os.path.join(root, rel))
            o = imtool_format.post_pass(common.clang_format(binary, style, text), limit=limit, indent_width=width, features=feats)
            return common.count_lines(text), {m: common.changed_lines(text, o, m) for m in MODES}

        tot = {"lines": 0, **{m: 0 for m in MODES}}
        for n, ch in common.pmap(one, cfiles):
            tot["lines"] += n
            for m in MODES:
                tot[m] += ch[m]
        return tot

    rows = [("no post-pass", set()), ("braces only", {"braces"}), ("pointers only", {"pointers"}), ("operators only", {"operators"}), ("bodies only", {"bodies"}), ("all", {"braces", "pointers", "operators", "bodies"})]
    for label, feats in rows:
        r = with_features(feats)
        out.append(f"  {label:16s} " + "  ".join(f"{MODE_LABEL[m]} {pct(r, m):5.1f}%" for m in MODES))
    out.append("")

    out.append("# 3. What the tool still changes in the corpus")
    residual(root, cfmt, cfiles, out)

    for n, lib in enumerate(a.library):
        lib_root = os.path.abspath(lib)
        lfiles = common.list_files(lib_root)
        out.append(f"# 4.{n + 1} Library tree: {os.path.basename(lib_root)} ({len(lfiles)} files)")
        columns = []
        if a.baseline_config:
            columns.append(("baseline config", measure.measure(binary, os.path.abspath(a.baseline_config), lib_root, lfiles)))
        columns.append(("config alone", measure.measure(binary, style, lib_root, lfiles)))
        lfmt = run_tree("library", lib_root, lfiles, os.path.join(work, "library%d" % n), binary, style, out, columns)
        for spec in a.excerpt:
            rel, _, pattern = spec.partition("::")
            if os.path.isfile(os.path.join(lib_root, rel)):
                excerpt(rel, pattern, a.excerpt_lines, lib_root, lfmt, out)
    print("\n".join(out))


if __name__ == "__main__":
    main()
