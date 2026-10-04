#!/usr/bin/env python3
"""Corpus survey: counts what the owner's hand-formatted code does, per project.

usage: survey.py --corpus DIR [--exclude PREFIX ...] > survey.txt
Token-based (comments, strings, preprocessor lines are not scanned for layout).
"""

import argparse
import collections
import os
import re
import sys

# The scripts import each other by their path from the repository root, so that the imports
# resolve the same way for Python and for a type checker run from the root.
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from toolkit.tools.format.fit import common  # noqa: E402
from toolkit.tools.format.imtool_format import lex  # noqa: E402

CTRL = ("if", "for", "while", "switch", "catch")
QUALS = {"const", "noexcept", "override", "final", "volatile", "mutable", "&", "&&"}
TYPE_KW = {
    "int", "float", "double", "char", "bool", "void", "auto", "unsigned", "long", "short", "size_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t", "string",
}  # fmt: skip


class Tok:
    __slots__ = ("kind", "text", "line", "col", "idx")

    def __init__(self, kind, text, line, col, idx):
        self.kind, self.text, self.line, self.col, self.idx = kind, text, line, col, idx


class File:
    """Tokens with positions, significant-token view, line table."""

    def __init__(self, text):
        self.text = text
        self.lines = text.split("\n")
        self.all = []
        line, col = 0, 0
        for i, (k, t) in enumerate(lex(text)):
            self.all.append(Tok(k, t, line, col, i))
            nls = t.count("\n")
            if nls:
                line += nls
                col = len(t) - t.rfind("\n") - 1
            else:
                col += len(t)
        self.sig = [t for t in self.all if t.kind in ("id", "num", "op", "str", "chr")]
        self.first_on_line = {}  # line -> first token of any non-ws kind
        for t in self.all:
            if t.kind not in ("ws", "nl") and t.line not in self.first_on_line:
                self.first_on_line[t.line] = t
        # matching brackets over significant tokens
        self.match = {}
        stack = []
        for i, t in enumerate(self.sig):
            if t.kind != "op":
                continue
            if t.text in "([{" and len(t.text) == 1:
                stack.append(i)
            elif t.text in ")]}" and len(t.text) == 1:
                want = {")": "(", "]": "[", "}": "{"}[t.text]
                # tolerate imbalance: pop until the matching opener kind
                while stack and self.sig[stack[-1]].text != want:
                    stack.pop()
                if stack:
                    j = stack.pop()
                    self.match[i] = j
                    self.match[j] = i

    def indent(self, line):
        s = self.lines[line]
        return len(s) - len(s.lstrip(" \t"))

    def is_first(self, tok):
        return self.first_on_line.get(tok.line) is tok

    def span_text(self, a, b):
        """Source text from sig[a] through sig[b], whitespace runs collapsed, comments dropped."""
        parts = []
        for t in self.all[self.sig[a].idx : self.sig[b].idx + 1]:
            if t.kind in ("ws", "nl"):
                if parts and parts[-1] != " ":
                    parts.append(" ")
            elif t.kind in ("lc", "bc", "pp"):
                continue
            else:
                parts.append(t.text)
        return "".join(parts).strip()

    def has_comment(self, a, b):
        return any(
            t.kind in ("lc", "bc", "pp")
            for t in self.all[self.sig[a].idx : self.sig[b].idx + 1]
        )


def head_start(f, i):
    """Index of the first sig token of the statement that sig[i] ('{') ends; flag if inside parens."""
    depth = 0
    k = i - 1
    while k >= 0:
        t = f.sig[k]
        if t.kind == "op":
            if t.text in (")", "]"):
                depth += 1
            elif t.text in ("(", "["):
                if depth == 0:
                    return k + 1, True
                depth -= 1
            elif depth == 0 and t.text in (";", "{", "}"):
                return k + 1, False
            elif depth == 0 and t.text == ",":
                # comma at depth 0: stop for initializer contexts, keep going for ctor-init lists (decided later)
                pass
        k -= 1
    return 0, False


def classify(f, i, scope):
    """Kind of the construct whose opening brace is sig[i]; returns (kind, head_first_index or None)."""
    start, in_parens = head_start(f, i)
    head = list(range(start, i))
    after_case = False
    # strip labels
    while head:
        t0 = f.sig[head[0]].text
        if (
            t0 in ("public", "private", "protected")
            and len(head) > 1
            and f.sig[head[1]].text == ":"
        ):
            head = head[2:]
            continue
        if t0 in ("case", "default"):
            d = 0
            cut = None
            for n, h in enumerate(head):
                tx = f.sig[h].text
                if tx in ("(", "["):
                    d += 1
                elif tx in (")", "]"):
                    d -= 1
                elif tx == ":" and d == 0:
                    cut = n
                    break
            if cut is None:
                break
            head = head[cut + 1 :]
            after_case = True
            continue
        break
    if not head:
        if after_case:
            return "case-block", i - 1
        if in_parens or (scope and scope[-1] == "init-list"):
            return "init-list", None
        return "bare-block", None
    texts = [f.sig[h].text for h in head]
    last = texts[-1]
    if texts[0] == "namespace" or texts[:2] == ["inline", "namespace"]:
        return "namespace", head[0]
    if texts[0] == "extern" and len(texts) == 2:
        return "extern-block", head[0]
    if last == "else":
        return "else", head[-1]
    if last == "do":
        return "do", head[-1]
    if last == "try":
        return "try", head[-1]
    # depth-0 scan (parens and angle brackets approximated)
    d = 0
    top = []
    for n, tx in enumerate(texts):
        if tx in ("(", "["):
            d += 1
        elif tx in (")", "]"):
            d -= 1
        elif d == 0:
            top.append((n, tx))
    top_texts = [tx for _, tx in top]
    # record types (skip a leading template<...>)
    if not in_parens and "=" not in top_texts:
        for kw in ("enum", "class", "struct", "union"):
            if kw in top_texts:
                n = [n for n, tx in top if tx == kw][0]
                # 'template<class T> void f() {' has class inside <>: require no '(' after the keyword at depth 0
                if "(" not in texts[n:]:
                    if (
                        kw in ("class", "struct")
                        and scope
                        and scope[-1] in ("class", "struct", "nested-class")
                    ):
                        return "nested-" + ("class"), head[0]
                    return kw, head[0]
    # trailing qualifiers / trailing return type / ctor-init list
    end = len(texts)
    cut_ctor = None
    for n, tx in top:
        if (
            tx == ":"
            and n > 0
            and texts[n - 1] in (")", "const", "noexcept", "override")
            and "?" not in texts[:n]
        ):
            cut_ctor = n
            break
    ctor_init = cut_ctor is not None
    if ctor_init:
        end = cut_ctor
    else:
        arrows = [n for n, tx in top if tx == "->"]
        if (
            arrows
            and arrows[-1] > 0
            and texts[arrows[-1] - 1] in (")", "mutable", "noexcept", "const")
        ):
            end = arrows[-1]
    while end > 0 and texts[end - 1] in QUALS:
        end -= 1
    if end > 0 and texts[end - 1] == ")":
        close = head[end - 1]
        op = f.match.get(close)
        if op is not None and op > 0:
            before = f.sig[op - 1].text
            if before in CTRL and not ctor_init:
                if before == "if" and op >= 2 and f.sig[op - 2].text == "else":
                    return "else-if", op - 2
                return before, op - 1
            if before == "]":
                return "lambda", f.match.get(op - 1, op - 1)
            if before in ("=", ",", "(", "return") or in_parens:
                return "init-list", head[0]
            if scope and scope[-1] in ("class", "struct", "nested-class"):
                return "inline-member-function", head[0]
            if scope and scope[-1] in (
                "function",
                "inline-member-function",
                "lambda",
                "if",
                "else",
                "else-if",
                "for",
                "while",
                "do",
                "switch",
                "case-block",
                "bare-block",
                "try",
                "catch",
            ):
                return "init-list", head[0]
            return "function", head[0]
    if last == "]":
        op = f.match.get(head[-1])
        if (
            op is not None
            and op > 0
            and f.sig[op - 1].text in ("=", "(", ",", "return", "{")
        ):
            return "lambda", op
        if op is not None and op == head[0] and in_parens:
            return "lambda", op
    return "init-list", head[0]


def body_stats(f, o, c):
    """Statement count at depth 0 inside sig[o]..sig[c], nested-control flag."""
    d = 0
    stmts = 0
    nested = False
    k = o + 1
    while k < c:
        tx = f.sig[k].text
        if f.sig[k].kind == "op":
            if tx in ("(", "["):
                d += 1
            elif tx in (")", "]"):
                d -= 1
            elif tx == "{":
                m = f.match.get(k)
                if m is None:
                    break
                prev = f.sig[k - 1].text
                if prev in (")", "else", "do", "try") or prev in (";", "{", "}"):
                    nested = True
                    # a nested block ends a statement unless followed by ';' or else/while
                    nxt = f.sig[m + 1].text if m + 1 < c else ""
                    if d == 0 and nxt not in (";", ",", ")", "else", "while"):
                        stmts += 1
                k = m
            elif tx == ";" and d == 0:
                stmts += 1
        k += 1
    return stmts, nested


class Counts:
    def __init__(self):
        self.c = collections.defaultdict(
            lambda: collections.Counter()
        )  # table -> (project,key) -> n
        self.examples = {}

    def add(self, table, project, key, n=1, example=None):
        self.c[table][(project, key)] += n
        if example is not None:
            self.examples.setdefault((table, key), example)

    def report(self, table, title=None, note=None, show_examples=False):
        data = self.c.get(table, {})
        keys = collections.Counter()
        for (_p, k), n in data.items():
            keys[k] += n
        projects = sorted({p for p, _ in data})
        out = ["", "## " + (title or table)]
        if note:
            out.append(note)
        total = sum(keys.values())
        short = [p[:12] for p in projects]
        out.append(
            f"{'':46s} {'total':>7s} {'share':>6s}  "
            + " ".join(f"{s:>12s}" for s in short)
        )
        for k, n in keys.most_common():
            out.append(
                f"{str(k)[:46]:46s} {n:7d} {100.0 * n / max(1, total):5.1f}%  "
                + " ".join(f"{data.get((p, k), 0):12d}" for p in projects)
            )
            if show_examples and (table, k) in self.examples:
                out.append("      e.g. " + self.examples[(table, k)])
        return "\n".join(out)


def brace_form(f, o, c, head_first):
    """Describe placement of the brace sig[o] relative to the head's first line."""
    ob, cb = f.sig[o], f.sig[c]
    own = f.is_first(ob)
    one_line = ob.line == cb.line
    if head_first is None:
        base = None
    else:
        base = f.indent(f.sig[head_first].line)
    if own:
        delta = ob.col - base if base is not None else 0
        place = f"own-line {delta:+d}" if delta in (0, 2, 4) else "own-line other"
    else:
        place = "same-line"
    return place, one_line, own, base


def survey_file(f, project, C, rows):
    scope = []  # kinds of enclosing braces
    scope_idx = []
    kind_of = {}
    sig = f.sig
    for i, t in enumerate(sig):
        if t.kind != "op":
            continue
        if t.text == "}":
            if scope:
                scope.pop()
                scope_idx.pop()
            continue
        if t.text != "{":
            continue
        kind, hf = classify(f, i, scope)
        kind_of[i] = kind
        c = f.match.get(i)
        scope.append(kind)
        scope_idx.append(i)
        if c is None:
            continue
        place, one_line, own, base = brace_form(f, i, c, hf)
        empty = c == i + 1
        table_kind = kind
        if kind == "lambda":
            # lambda braces are reported relative to the statement line and relative to the '[' column
            st, _ = head_start(f, hf)
            stmt_line = sig[st].line if st < len(sig) else t.line
            if not own:
                key = (
                    "same-line, one-line body"
                    if one_line
                    else "same-line, expanded body"
                )
            else:
                rel_stmt = t.col - f.indent(stmt_line)
                rel_br = t.col - sig[hf].col
                key = (
                    f"own-line, brace at '[' column{rel_br:+d}"
                    if rel_br in (0, 2)
                    else f"own-line, brace at statement indent{rel_stmt:+d}"
                    if rel_stmt in (0, 2, 4)
                    else "own-line, other column"
                )
                key += ", one-line body" if one_line else ", expanded"
            C.add("brace:lambda", project, key, example=f.lines[t.line].strip()[:90])
            continue
        if kind == "init-list":
            if own:
                key = "brace on its own line"
            else:
                key = "brace on the statement line, " + (
                    "closed on the same line" if one_line else "multi-line"
                )
            C.add("brace:init-list", project, key)
            if one_line and not empty:
                inner_l = f.all[t.idx + 1].kind == "ws"
                inner_r = f.all[sig[c].idx - 1].kind == "ws"
                C.add(
                    "space:init-braces",
                    project,
                    {
                        (True, True): "{ x } spaces inside",
                        (False, False): "{x} no spaces inside",
                    }.get((inner_l, inner_r), "mixed"),
                )
                prev = f.all[t.idx - 1]
                if sig[i - 1].kind == "id" or sig[i - 1].text == ">":
                    C.add(
                        "space:before-init-brace",
                        project,
                        "T x {..} space before brace"
                        if prev.kind == "ws"
                        else "T x{..} no space before brace",
                    )
            elif empty:
                C.add(
                    "space:empty-init-braces",
                    project,
                    "{}" if f.all[t.idx + 1] is sig[c] else "{ }",
                )
            continue
        key = place + (", closed on the same line" if one_line else ", expanded")
        C.add("brace:" + table_kind, project, key, example=None)
        # body indentation relative to the head
        if not one_line and base is not None and not empty:
            first_body = sig[i + 1]
            if first_body.line > t.line and f.is_first(first_body):
                C.add(
                    "bodyindent:" + table_kind,
                    project,
                    f"body {first_body.col - base:+d} from head",
                )
        # empty bodies of functions
        if kind in ("function", "inline-member-function"):
            if empty:
                between = f.text and "".join(
                    x.text for x in f.all[t.idx + 1 : sig[c].idx]
                )
                if one_line:
                    form = ("{ } " if between else "{} ") + (
                        "on the declaration line" if not own else "on its own line"
                    )
                else:
                    form = "expanded empty block (brace / blank / brace)"
                C.add("empty-function-body", project, form)
            # ctor initializer list layout
            st, _ = head_start(f, i)
            texts = [sig[h].text for h in range(st, i)]
            d = 0
            colon = None
            for n, tx in enumerate(texts):
                if tx in ("(", "["):
                    d += 1
                elif tx in (")", "]"):
                    d -= 1
                elif (
                    tx == ":"
                    and d == 0
                    and n > 0
                    and texts[n - 1] == ")"
                    and "?" not in texts[:n]
                ):
                    colon = st + n
                    break
            if colon is not None:
                ct = sig[colon]
                inits = [colon + 1]
                d = 0
                for h in range(colon + 1, i):
                    tx = sig[h].text
                    if tx in ("(", "[", "{"):
                        d += 1
                    elif tx in (")", "]", "}"):
                        d -= 1
                    elif tx == "," and d == 0:
                        inits.append(h + 1)
                colon_pos = (
                    "colon starts a new line"
                    if f.is_first(ct)
                    else (
                        "colon ends the line"
                        if sig[colon + 1].line > ct.line
                        else "colon on the signature line"
                    )
                )
                if f.is_first(ct):
                    C.add("ctor-init-indent", project, f"colon line {ct.col - f.indent(sig[st].line):+d} from the signature")
                if len(inits) == 1:
                    C.add("ctor-init", project, f"single initializer, {colon_pos}")
                else:
                    lines = {sig[h].line for h in inits}
                    commas_lead = sum(1 for h in inits[1:] if f.is_first(sig[h - 1]))
                    if len(lines) == 1:
                        lay = "all on one line"
                    elif len(lines) == len(inits):
                        lay = "one per line, " + (
                            "leading commas"
                            if commas_lead == len(inits) - 1
                            else "trailing commas"
                        )
                    else:
                        lay = "packed over several lines, " + (
                            "leading commas" if commas_lead else "trailing commas"
                        )
                    C.add(
                        "ctor-init",
                        project,
                        f"{len(inits) > 1 and 'multiple'} initializers, {colon_pos}, {lay}",
                    )
        # namespace body indentation
        if kind == "namespace" and not one_line and not empty:
            fb = sig[i + 1]
            if f.is_first(fb):
                C.add(
                    "namespace-body-indent",
                    project,
                    f"body {fb.col - f.indent(sig[hf].line):+d} from 'namespace'",
                )
        # access specifier / case label indentation
        if kind in ("class", "struct", "nested-class"):
            d = 0
            for h in range(i + 1, c):
                tx = sig[h].text
                if tx == "{":
                    d += 1
                elif tx == "}":
                    d -= 1
                elif (
                    d == 0
                    and tx in ("public", "private", "protected")
                    and sig[h + 1].text == ":"
                    and f.is_first(sig[h])
                ):
                    C.add(
                        "access-specifier-indent",
                        project,
                        f"{sig[h].col - base:+d} from '{'class' if kind != 'struct' else 'struct'}'",
                    )
        if kind == "switch":
            d = 0
            for h in range(i + 1, c):
                tx = sig[h].text
                if tx == "{":
                    d += 1
                elif tx == "}":
                    d -= 1
                elif d == 0 and tx in ("case", "default") and f.is_first(sig[h]):
                    C.add(
                        "case-label-indent",
                        project,
                        f"case {sig[h].col - base:+d} from 'switch' ({sig[h].col - t.col:+d} from its brace)",
                    )
                    # is the case body on the label line?
                    e = h
                    pd = 0
                    while e < c and not (sig[e].text == ":" and pd == 0):
                        pd += sig[e].text in ("(", "[")
                        pd -= sig[e].text in (")", "]")
                        e += 1
                    if e + 1 < c:
                        nxt = sig[e + 1]
                        if nxt.text in ("case", "default"):
                            pass
                        elif nxt.line == sig[e].line:
                            C.add(
                                "case-body",
                                project,
                                "statement(s) on the label line"
                                if nxt.text != "{"
                                else "braced block starts on the label line",
                            )
                        else:
                            C.add(
                                "case-body",
                                project,
                                "body on following lines"
                                if nxt.text != "{"
                                else "braced block on following lines",
                            )
        # one-line / two-liner / expanded choice for control statements
        if kind in ("if", "else-if", "else", "for", "while", "function", "inline-member-function"):
            stmts, nested = body_stats(f, i, c)
            last_head = sig[i - 1]
            head_text = f.span_text(hf, i - 1)
            body_text = f.span_text(i + 1, c - 1) if c > i + 1 else ""
            comment = f.has_comment(hf, c)
            if not own and one_line:
                form = "one-line"
            elif own and one_line:
                form = "two-liner"
            elif own:
                form = "expanded"
            else:
                form = "attached-expanded"
            assert base is not None  # these kinds always have a head
            L1 = base + len(head_text) + 1 + len("{ " + body_text + " }")
            L2 = base + 2 + len("{ " + body_text + " }")
            rows.append(
                dict(project=project, kind=kind, form=form, stmts=stmts, nested=nested, comment=comment, L1=L1, L2=L2,
                     headlen=base + len(head_text), multi_head=last_head.line != sig[hf].line, body=body_text, head=head_text,
                     first=sig[i + 1].text if c > i + 1 else "", line=sig[hf].line, open=i, close=c, fid=id(f), hf=hf)
            )  # fmt: skip
    # braceless control statements and else placement
    for i, t in enumerate(sig):
        if t.kind != "id":
            continue
        if (
            t.text in ("if", "for", "while")
            and i + 1 < len(sig)
            and sig[i + 1].text == "("
        ):
            c = f.match.get(i + 1)
            if c is None or c + 1 >= len(sig):
                continue
            nxt = sig[c + 1]
            if (
                t.text == "while"
                and i > 0
                and sig[i - 1].text == "}"
                and kind_of.get(f.match.get(i - 1)) == "do"
            ):
                continue
            kindname = (
                "else-if"
                if (t.text == "if" and i > 0 and sig[i - 1].text == "else")
                else t.text
            )
            if nxt.text == "{":
                C.add("braces-on-control", project, f"{kindname}: braced")
            elif nxt.text == ";":
                C.add("braces-on-control", project, f"{kindname}: empty body ';'")
            else:
                C.add(
                    "braces-on-control",
                    project,
                    f"{kindname}: NO braces, "
                    + (
                        "statement on the same line"
                        if nxt.line == sig[c].line
                        else "statement on the next line"
                    ),
                )
        elif t.text == "else":
            nxt = sig[i + 1] if i + 1 < len(sig) else None
            if nxt is not None and nxt.text not in ("{", "if"):
                C.add("braces-on-control", project, "else: NO braces")
            elif nxt is not None and nxt.text == "{":
                C.add("braces-on-control", project, "else: braced")
            if i > 0 and sig[i - 1].text == "}":
                C.add(
                    "else-placement",
                    project,
                    "else on its own line after '}'"
                    if f.is_first(t)
                    else "'} else' on the closing-brace line",
                )
            else:
                C.add("else-placement", project, "else after a braceless statement")
        elif t.text == "do" and i + 1 < len(sig) and sig[i + 1].text == "{":
            c = f.match.get(i + 1)
            if c is not None and c + 1 < len(sig) and sig[c + 1].text == "while":
                C.add(
                    "do-while-tail",
                    project,
                    "'} while(..);' on the closing-brace line"
                    if sig[c + 1].line == sig[c].line
                    else "'while(..);' on its own line",
                )
    return kind_of


# ---------------------------------------------------------------- token spacing habits


def survey_spacing(f, project, C):
    sig = f.sig
    allt = f.all

    def ws_before(t):
        p = allt[t.idx - 1] if t.idx > 0 else None
        return p is not None and p.kind in ("ws", "nl")

    def ws_after(t):
        n = allt[t.idx + 1] if t.idx + 1 < len(allt) else None
        return n is None or n.kind in ("ws", "nl")

    for i, t in enumerate(sig):
        prev = sig[i - 1] if i > 0 else None
        nxt = sig[i + 1] if i + 1 < len(sig) else None
        if (
            t.kind == "id"
            and t.text in ("if", "for", "while", "switch", "catch")
            and nxt is not None
            and nxt.text == "("
        ):
            C.add(
                "space:control-paren",
                project,
                f"{t.text} (" if ws_before(nxt) else f"{t.text}(",
            )
        if (
            t.kind == "id"
            and t.text == "template"
            and nxt is not None
            and nxt.text == "<"
        ):
            C.add(
                "space:template",
                project,
                "template <" if ws_before(nxt) else "template<",
            )
            m = None
            # template<...> on its own line?
            d = 0
            for h in range(i + 1, min(i + 200, len(sig))):
                if sig[h].text == "<":
                    d += 1
                elif sig[h].text == ">":
                    d -= 1
                    if d == 0:
                        m = h
                        break
                elif sig[h].text == ">>":
                    d -= 2
                    if d <= 0:
                        m = h
                        break
            if m is not None and m + 1 < len(sig):
                C.add(
                    "template-line",
                    project,
                    "declaration continues on the template<> line"
                    if sig[m + 1].line == sig[m].line
                    else "template<> on its own line",
                )
        if t.kind != "op":
            if (
                t.kind == "id"
                and prev is not None
                and prev.kind == "op"
                and prev.text == ")"
                and t.text not in QUALS
                and t.text not in ("override", "final")
            ):
                # C-style cast: '(' type ')' operand
                o = f.match.get(i - 1)
                if o is not None and i - 1 - o in (2, 3, 4):
                    inner = [x.text for x in sig[o + 1 : i - 1]]
                    before = sig[o - 1] if o > 0 else None
                    if (
                        inner
                        and inner[0] in TYPE_KW | {"const"}
                        and all(x in TYPE_KW | {"*", "const", "&"} for x in inner)
                        and (
                            before is None
                            or before.kind == "op"
                            or before.text == "return"
                        )
                    ):
                        C.add(
                            "space:after-c-cast",
                            project,
                            "(T) x" if ws_before(t) else "(T)x",
                        )
            continue
        tx = t.text
        if tx == ",":
            if nxt is not None and nxt.line == t.line:
                C.add(
                    "space:after-comma",
                    project,
                    "', ' space after" if ws_after(t) else "',' no space after",
                )
            if ws_before(t) and not f.is_first(t):
                C.add("space:before-comma", project, "space before comma (alignment)")
        elif tx == "(" and nxt is not None and nxt.line == t.line and nxt.text != ")":
            C.add("space:inside-parens", project, "( x" if ws_after(t) else "(x")
            if (
                prev is not None
                and prev.kind == "id"
                and prev.text not in CTRL
                and prev.text
                not in (
                    "return",
                    "sizeof",
                    "else",
                    "and",
                    "or",
                    "not",
                    "case",
                    "throw",
                    "delete",
                    "new",
                    "operator",
                )
            ):
                C.add("space:call-paren", project, "f (" if ws_before(t) else "f(")
        elif (
            tx == ")" and prev is not None and prev.line == t.line and prev.text != "("
        ):
            C.add("space:inside-parens-close", project, "x )" if ws_before(t) else "x)")
        elif tx in (
            "==",
            "!=",
            "<=",
            ">=",
            "&&",
            "||",
            "=",
            "+=",
            "-=",
            "*=",
            "/=",
            "+",
            "-",
            "*",
            "/",
            "%",
            "<<",
            ">>",
            "?",
        ):
            binary = prev is not None and (
                prev.kind in ("id", "num", "str", "chr") or prev.text in (")", "]")
            )
            skip = (
                prev is None
                or nxt is None
                or prev.line != t.line
                or nxt.line != t.line
                or (
                    tx in ("+", "-", "*")
                    and (
                        not binary
                        or prev.text in ("return", "case", "const")
                        or prev.text in TYPE_KW
                    )
                )
                or (
                    tx in ("*", ">>", "<<")
                    and (
                        prev.text == ">"
                        or (
                            prev.kind == "id"
                            and (prev.text[:1].isupper() or prev.text.endswith("_t"))
                        )
                    )
                )
                or (tx == "=" and prev.text in ("operator", "[", "<", ">", "!", "="))
                or prev.text == "operator"
            )
            if not skip:
                b, a = ws_before(t), ws_after(t)
                grp = {"=": "assignment =", "+=": "compound assignment", "-=": "compound assignment", "*=": "compound assignment", "/=": "compound assignment",
                       "==": "comparison", "!=": "comparison", "<=": "comparison", ">=": "comparison", "&&": "logical && ||", "||": "logical && ||",
                       "+": "additive + -", "-": "additive + -", "*": "multiplicative * / %", "/": "multiplicative * / %", "%": "multiplicative * / %",
                       "<<": "shift/stream << >>", ">>": "shift/stream << >>", "?": "ternary ?"}[tx]  # fmt: skip
                C.add(
                    "space:binary-operators",
                    project,
                    grp + ": " + ("a op b" if b and a else "aopb" if not b and not a else "asymmetric"),
                )
        # pointer / reference binding
        if (
            tx in ("*", "&", "&&")
            and prev is not None
            and nxt is not None
            and prev.line == t.line
        ):
            b, a = ws_before(t), ws_after(t)
            prev_typeish = (
                prev.text in TYPE_KW
                or prev.text == ">"
                or prev.text == "const"
                or (
                    prev.kind == "id"
                    and (prev.text[:1].isupper() or prev.text.endswith("_t"))
                )
            )
            if (
                nxt.text in (">", ",", ">>")
                and prev_typeish
                and prev.kind in ("id", "op")
            ):
                # inside template arguments or a parameter list without a name
                o = None
                d = 0
                for h in range(i - 1, max(-1, i - 40), -1):
                    x = sig[h].text
                    if x in (">", ")"):
                        d += 1
                    elif x in ("<", "("):
                        if d == 0:
                            o = sig[h]
                            break
                        d -= 1
                    elif x in (";", "{", "}"):
                        break
                if o is not None and o.text == "<":
                    C.add(
                        "pointer:in-template-args",
                        project,
                        f"<T {tx}> space before" if b else f"<T{tx}> no space",
                    )
                    continue
            if nxt.text == ")" and prev_typeish:
                o = f.match.get(i + 1)
                if (
                    o is not None
                    and i - o <= 4
                    and (
                        o == 0 or sig[o - 1].kind == "op" or sig[o - 1].text == "return"
                    )
                ):
                    C.add(
                        "pointer:in-c-cast",
                        project,
                        f"(T {tx}) space before" if b else f"(T{tx}) no space",
                    )
                    continue
            if nxt.kind == "id" and prev_typeish and nxt.text not in QUALS:
                after = sig[i + 2].text if i + 2 < len(sig) else ""
                is_ret = after in ("(", "::") or nxt.text == "operator"
                if prev.text == ">" and not (b != a):
                    pass
                if b and not a:
                    form = "T *x (binds right)"
                elif a and not b:
                    form = "T* x (binds left)"
                elif a and b:
                    form = "T * x (both sides)"
                else:
                    form = "T*x (no spaces)"
                if (
                    tx != "*"
                    and form in ("T * x (both sides)", "T*x (no spaces)")
                    and prev.text not in TYPE_KW
                    and prev.text != "const"
                ):
                    continue  # a & b / a && b expressions
                if (
                    tx == "*"
                    and form in ("T * x (both sides)", "T*x (no spaces)")
                    and prev.text not in TYPE_KW
                    and prev.text not in ("const", ">")
                ):
                    continue  # multiplication
                form = form.replace("*", tx)
                if is_ret:
                    C.add("pointer:return-type-or-qualified-name", project, form)
                else:
                    C.add(
                        "pointer:declaration (variable, parameter, member)",
                        project,
                        form,
                    )


# ---------------------------------------------------------------- line habits

ASSIGN_RE = re.compile(r"^[^=!<>+\-*/%&|^(]*?[^=!<>+\-*/%&|^ ]( *)(=)(?!=)( *)(\S?)")
DECL_RE = re.compile(
    r"^(\s*)((?:const |static |unsigned |mutable |constexpr |inline )*[A-Za-z_][\w:<>,*& ]*?[\w>*&])( +)([*&]*[A-Za-z_]\w*)\s*(=|;|\{|\[|\()"
)


def survey_lines(f, project, C, kind_of):
    lines = f.lines
    # which lines are code lines (start outside multi-line tokens, not preprocessor, not pure comment)
    line_kind = {}
    for t in f.all:
        if t.kind in ("ws", "nl"):
            continue
        if t.line not in line_kind:
            line_kind[t.line] = t.kind
        if "\n" in t.text:
            for extra in range(1, t.text.count("\n") + 1):
                line_kind.setdefault(t.line + extra, "cont")
    n_code = sum(1 for k in line_kind.values() if k not in ("lc", "bc", "pp", "cont"))
    C.add("lines", project, "code lines (not comment / preprocessor)", n_code)
    C.add("lines", project, "all lines", len(lines))
    for ln, s in enumerate(lines):
        if s.strip() == "" and s != "":
            C.add("trailing-whitespace", project, "whitespace-only line")
        elif s != s.rstrip():
            C.add("trailing-whitespace", project, "trailing whitespace after text")
        if len(s) > 140:
            C.add("line-length", project, "longer than 140 columns")
        elif len(s) > 100:
            C.add("line-length", project, "101..140 columns")
        elif s.strip():
            C.add("line-length", project, "1..100 columns")
        if "\t" in s:
            C.add("tabs", project, "line contains a tab")
        ind = len(s) - len(s.lstrip(" "))
        if s.strip() and line_kind.get(ln) not in ("lc", "bc", "pp", "cont"):
            C.add(
                "indent-width",
                project,
                "indent multiple of 2"
                if ind % 2 == 0
                else "odd indent (alignment / continuation)",
            )

    def code(ln):
        return line_kind.get(ln) not in (None, "lc", "bc", "pp", "cont")

    # ---- '=' alignment between neighbouring lines
    def eq_col(ln):
        if not code(ln):
            return None
        s = lines[ln]
        if s.lstrip().startswith(("for(", "for (", "if(", "if (", "while(", "return")):
            return None
        m = ASSIGN_RE.match(s)
        if not m:
            return None
        return m.start(2), len(m.group(1)), len(m.group(3)), m.group(4)

    run = []

    def flush(run):
        if len(run) < 2:
            if len(run) == 1:
                C.add("align:=", project, "isolated assignment line (no neighbour)")
            return
        cols = {r[0] for r in run}
        padded = any(r[1] > 1 for r in run)
        if len(cols) == 1 and padded:
            C.add(
                "align:=",
                project,
                "lines in a run aligned on '=' with padding",
                len(run),
            )
            C.add("align:= (runs)", project, "aligned run")
            # sign offset: extra space after '=' when a neighbour has a minus sign
            if any(r[3] == "-" for r in run):
                if any(r[2] == 2 for r in run if r[3] != "-"):
                    C.add(
                        "align:sign-offset",
                        project,
                        "run with '-' values: positives get an extra space ('=  1' / '= -1')",
                    )
                else:
                    C.add(
                        "align:sign-offset",
                        project,
                        "run with '-' values: no sign offset",
                    )
        elif len(cols) == 1:
            C.add(
                "align:=",
                project,
                "lines in a run whose '=' coincide without padding",
                len(run),
            )
        else:
            C.add("align:=", project, "lines in a run NOT aligned on '='", len(run))
            C.add("align:= (runs)", project, "unaligned run")

    for ln in range(len(lines)):
        e = eq_col(ln)
        if e is None:
            flush(run)
            run = []
        else:
            run.append(e)
    flush(run)

    # ---- declaration-name alignment (members / locals)
    run = []

    def flush_decl(run):
        if len(run) < 2:
            return
        cols = {r[0] for r in run}
        padded = any(r[1] > 1 for r in run)
        if len(cols) == 1 and padded:
            C.add(
                "align:declaration-names",
                project,
                "lines in a run with names aligned by padding",
                len(run),
            )
        elif len(cols) == 1:
            C.add(
                "align:declaration-names",
                project,
                "lines in a run, names coincide without padding",
                len(run),
            )
        else:
            C.add(
                "align:declaration-names",
                project,
                "lines in a run, names NOT aligned",
                len(run),
            )

    for ln in range(len(lines)):
        m = DECL_RE.match(lines[ln]) if code(ln) else None
        if m and m.group(2).split()[0] not in (
            "return",
            "delete",
            "else",
            "case",
            "using",
            "typedef",
            "new",
            "throw",
            "goto",
        ):
            run.append((m.start(4), len(m.group(3))))
        else:
            flush_decl(run)
            run = []
    flush_decl(run)

    # ---- trailing comment alignment
    tc = {}
    for t in f.all:
        if t.kind == "lc" and not f.is_first(t):
            tc[t.line] = (
                t.col,
                f.all[t.idx - 1].kind == "ws" and len(f.all[t.idx - 1].text) > 1,
            )
    seen = set()
    for ln in sorted(tc):
        if ln in seen:
            continue
        run = [ln]
        while run[-1] + 1 in tc:
            run.append(run[-1] + 1)
        seen.update(run)
        if len(run) < 2:
            C.add("align:trailing-comments", project, "isolated trailing comment")
            continue
        cols = {tc[r][0] for r in run}
        if len(cols) == 1:
            C.add(
                "align:trailing-comments",
                project,
                "lines in a run with comments in one column",
                len(run),
            )
        else:
            C.add(
                "align:trailing-comments",
                project,
                "lines in a run with comments NOT in one column",
                len(run),
            )

    # ---- argument alignment: padding after a comma or '(' inside a line; continuation lines aligned to '('
    for ln, s in enumerate(lines):
        if not code(ln):
            continue
        body = s.strip()
        if re.search(r",\s{2,}\S", body):
            C.add(
                "align:arguments",
                project,
                "line with 2+ spaces after a comma (column-aligned arguments)",
            )
        if re.search(r"\S\s{2,}[^\s/]", body):
            C.add("align:any", project, "code line with 2+ consecutive spaces mid-line")
        else:
            C.add("align:any", project, "code line without mid-line padding")
    # continuation lines
    depth_stack = []
    for t in f.sig:
        if t.kind == "op" and t.text == "(":
            depth_stack.append(t)
        elif t.kind == "op" and t.text == ")":
            if depth_stack:
                depth_stack.pop()
        elif depth_stack and f.is_first(t) and t.line > depth_stack[-1].line:
            o = depth_stack[-1]
            if t.col == o.col + 1:
                C.add(
                    "continuation", project, "argument continuation aligned after '('"
                )
            else:
                C.add(
                    "continuation",
                    project,
                    f"argument continuation at {t.col - f.indent(o.line):+d} from the statement"
                    if t.col - f.indent(o.line) in (2, 4)
                    else "argument continuation at another column",
                )

    # ---- blank lines between top-level functions
    sig = f.sig
    for i, t in enumerate(sig):
        if (
            t.text == "}"
            and t.kind == "op"
            and kind_of.get(f.match.get(i)) == "function"
            and i + 1 < len(sig)
        ):
            ln = t.line + 1
            blanks = 0
            while ln < len(lines) and lines[ln].strip() == "":
                blanks += 1
                ln += 1
            if ln < len(lines):
                C.add(
                    "blank-lines-after-function",
                    project,
                    f"{blanks} blank line(s)" if blanks < 4 else "4+ blank lines",
                )
    # blank line runs anywhere
    blanks = 0
    for s in lines:
        if s.strip() == "":
            blanks += 1
        else:
            if blanks:
                C.add(
                    "blank-line-runs",
                    project,
                    f"run of {blanks}" if blanks < 4 else "run of 4+",
                )
            blanks = 0

    # ---- includes
    incs = []
    for t in f.all:
        if t.kind == "pp":
            m = re.match(r'#\s*include\s*([<"])([^>"]+)[>"]', t.text)
            if m:
                incs.append((t.line, m.group(1), m.group(2)))
    if incs:
        kinds = "".join("s" if k == "<" else "p" for _, k, _ in incs)
        if "s" in kinds and "p" in kinds:
            if re.fullmatch(r"s+p+", kinds):
                C.add(
                    "includes:order", project, 'system <...> first, then project "..."'
                )
            elif re.fullmatch(r"p+s+", kinds):
                C.add(
                    "includes:order", project, 'project "..." first, then system <...>'
                )
            elif re.fullmatch(r"ps+p+", kinds):
                C.add(
                    "includes:order", project, "own header, then system, then project"
                )
            else:
                C.add("includes:order", project, "interleaved")
        else:
            C.add("includes:order", project, "only one kind of include")
        # contiguous groups
        groups = [[incs[0]]]
        for a, b in zip(incs, incs[1:]):
            if b[0] == a[0] + 1:
                groups[-1].append(b)
            else:
                groups.append([b])
        for g in groups:
            if len(g) < 2:
                continue
            names = [x[2].lower() for x in g]
            C.add(
                "includes:sorted",
                project,
                "contiguous group alphabetically sorted"
                if names == sorted(names)
                else "contiguous group NOT sorted",
            )
        C.add(
            "includes:groups",
            project,
            "file with 1 include group"
            if len(groups) == 1
            else "file with 2+ blank-line-separated groups",
        )


# ---------------------------------------------------------------- the one-line / two-liner / expanded rule


def fit_functions(rows, out):
    out.append("")
    out.append("## Function bodies: one-line vs two-liner vs expanded")
    forms = ("one-line", "two-liner", "expanded", "attached-expanded")
    for kind, title in (("function", "functions at namespace / file scope"), ("inline-member-function", "member functions defined in a class body")):
        sub = [r for r in rows if r["kind"] == kind]
        out.append(f"{title}: form by number of statements")
        out.append(f"{'':20s}" + "".join(f"{'stmts=' + (str(s) if s < 4 else '4+'):>12s}" for s in range(0, 5)))
        by = collections.Counter((r["form"], min(r["stmts"], 4)) for r in sub)
        for fm in forms:
            out.append(f"{fm:20s}" + "".join(f"{by[(fm, s)]:12d}" for s in range(0, 5)))
        single = [r for r in sub if r["stmts"] == 1 and not r["nested"] and not r["comment"] and not r["multi_head"]]
        out.append(f"  single statement, no nested block, no comment, signature on one line ({len(single)}): form by L1 (width on one line)")
        bk = collections.Counter(((min(r["L1"], 179) // 20) * 20, r["form"]) for r in single)
        for b in range(0, 180, 20):
            if any(bk[(b, fm)] for fm in forms):
                out.append(f"    L1 {b:3d}..{b + 19 if b < 160 else 999:<4d}" + "".join(f"{fm}={bk[(b, fm)]:<6d} " for fm in forms[:3]))
        n = max(1, len(single))
        for name, pred in (
            ("always one-line", lambda r: "one-line"),
            ("always expanded", lambda r: "expanded"),
            ("one-line if L1 <= 140, else two-liner if its body line <= 140, else expanded", lambda r: "one-line" if r["L1"] <= 140 else ("two-liner" if r["L2"] - 2 <= 140 else "expanded")),
            ("one-line if L1 <= 140, else expanded", lambda r: "one-line" if r["L1"] <= 140 else "expanded"),
            ("always two-liner", lambda r: "two-liner"),
        ):
            out.append(f"    {100 * sum(1 for r in single if pred(r) == r['form']) / n:5.1f}%  {name}")
        per = collections.Counter((r["project"], r["form"]) for r in single)
        projects = sorted({r["project"] for r in single})
        out.append("    per project (one-line / two-liner / expanded): " + ", ".join(f"{p[:10]} {per[(p, 'one-line')]}/{per[(p, 'two-liner')]}/{per[(p, 'expanded')]}" for p in projects))


def fit_rule(rows, out):
    rows = [r for r in rows if r["kind"] not in ("function", "inline-member-function")]
    out.append("")
    out.append(
        "## Rule 5: one-line vs two-liner vs expanded for braced control bodies (if / else-if / else / for / while)"
    )
    forms = ("one-line", "two-liner", "expanded", "attached-expanded")
    by = collections.Counter((r["form"], min(r["stmts"], 4)) for r in rows)
    out.append(
        "form by number of statements in the body (all bodies, comments included):"
    )
    out.append(
        f"{'':20s}"
        + "".join(f"{'stmts=' + (str(s) if s < 4 else '4+'):>12s}" for s in range(0, 5))
    )
    for fm in forms:
        out.append(f"{fm:20s}" + "".join(f"{by[(fm, s)]:12d}" for s in range(0, 5)))
    by = collections.Counter(
        (r["project"], r["form"]) for r in rows if r["stmts"] == 1 and not r["nested"]
    )
    projects = sorted({r["project"] for r in rows})
    out.append("")
    out.append("single-statement bodies (no nested block), by project:")
    out.append(f"{'':20s}" + "".join(f"{p[:12]:>13s}" for p in projects))
    for fm in forms:
        out.append(f"{fm:20s}" + "".join(f"{by[(p, fm)]:13d}" for p in projects))

    single = [
        r
        for r in rows
        if r["stmts"] == 1
        and not r["nested"]
        and not r["comment"]
        and r["form"] != "attached-expanded"
        and not r["multi_head"]
    ]
    out.append("")
    out.append(
        f"Population for the rule fit: single statement, no nested block, no comment, head on one line: {len(single)} bodies"
    )
    out.append(
        "L1 = column width the statement would have on one line: indent + head + ' { ' + body + ' }'"
    )
    out.append(f"{'L1 bucket':12s}{'one-line':>10s}{'two-liner':>10s}{'expanded':>10s}")
    bk = collections.Counter(
        ((min(r["L1"], 179) // 10) * 10, r["form"]) for r in single
    )
    for b in range(0, 180, 10):
        if any(bk[(b, fm)] for fm in forms):
            out.append(
                f"{str(b) + '..' + (str(b + 9) if b < 170 else '+'):12s}"
                + "".join(f"{bk[(b, fm)]:10d}" for fm in forms[:3])
            )
    out.append("")
    out.append("by statement kind (single statement):")
    kinds = collections.Counter()
    for r in single:
        k = (
            r["first"]
            if r["first"] in ("return", "break", "continue", "delete", "throw")
            else "other"
        )
        kinds[(k, r["form"])] += 1
    for k in ("return", "break", "continue", "delete", "throw", "other"):
        out.append(
            f"  {k:10s}" + "".join(f"{fm}={kinds[(k, fm)]:<7d} " for fm in forms[:3])
        )
    out.append("by control keyword (single statement):")
    kinds = collections.Counter((r["kind"], r["form"]) for r in single)
    for k in ("if", "else-if", "else", "for", "while"):
        out.append(
            f"  {k:10s}" + "".join(f"{fm}={kinds[(k, fm)]:<7d} " for fm in forms[:3])
        )

    n = len(single)

    def acc(pred):
        return sum(1 for r in single if pred(r) == r["form"]) / max(1, n)

    out.append("")
    out.append("candidate deterministic rules, accuracy on the population above:")
    cands = []
    cands.append(("always one-line", lambda r: "one-line"))
    cands.append(("always two-liner", lambda r: "two-liner"))
    cands.append(("always expanded", lambda r: "expanded"))
    cands.append(
        (
            "one-line for if/else-if/else, two-liner for loops",
            lambda r: "two-liner" if r["kind"] in ("for", "while") else "one-line",
        )
    )
    best = (-1.0, 0)
    for T in range(40, 161, 2):
        a = acc(lambda r, T=T: "one-line" if r["L1"] <= T else "two-liner")
        if a > best[0]:
            best = (a, T)
    cands.append(
        (
            f"one-line if L1 <= {best[1]} else two-liner (best threshold of 40..160)",
            lambda r, T=best[1]: "one-line" if r["L1"] <= T else "two-liner",
        )
    )
    best = (-1.0, 0)
    for T in range(40, 161, 2):
        a = acc(lambda r, T=T: "one-line" if r["L1"] <= T else "expanded")
        if a > best[0]:
            best = (a, T)
    cands.append(
        (
            f"one-line if L1 <= {best[1]} else expanded (best threshold)",
            lambda r, T=best[1]: "one-line" if r["L1"] <= T else "expanded",
        )
    )
    best = (-1.0, 0, 0)
    for T in range(40, 161, 4):
        for T2 in range(40, 161, 4):
            a = acc(
                lambda r, T=T, T2=T2: (
                    "one-line"
                    if r["L1"] <= T
                    else ("two-liner" if r["L2"] <= T2 else "expanded")
                )
            )
            if a > best[0]:
                best = (a, T, T2)
    cands.append(
        (
            f"one-line if L1 <= {best[1]}; else two-liner if its body line <= {best[2]}; else expanded",
            lambda r, T=best[1], T2=best[2]: (
                "one-line"
                if r["L1"] <= T
                else ("two-liner" if r["L2"] <= T2 else "expanded")
            ),
        )
    )
    best = (-1.0, 0)
    for T in range(20, 141, 2):
        a = acc(lambda r, T=T: "one-line" if r["headlen"] <= T else "two-liner")
        if a > best[0]:
            best = (a, T)
    cands.append(
        (
            f"one-line if the head (indent + condition) <= {best[1]} columns else two-liner",
            lambda r, T=best[1]: "one-line" if r["headlen"] <= T else "two-liner",
        )
    )
    best = (-1.0, 0)
    for T in range(40, 161, 2):
        a = acc(
            lambda r, T=T: (
                "two-liner"
                if (r["kind"] in ("for", "while") or r["L1"] > T)
                else "one-line"
            )
        )
        if a > best[0]:
            best = (a, T)
    cands.append(
        (
            f"two-liner for loops; for if/else: one-line if L1 <= {best[1]} else two-liner",
            lambda r, T=best[1]: (
                "two-liner"
                if (r["kind"] in ("for", "while") or r["L1"] > T)
                else "one-line"
            ),
        )
    )
    cands.append(
        (
            "AS IMPLEMENTED: one-line if L1 <= 140; else two-liner if its body line <= 140; else expanded",
            lambda r: "one-line" if r["L1"] <= 140 else ("two-liner" if r["L2"] <= 140 else "expanded"),
        )
    )
    for name, pred in cands:
        out.append(f"  {100 * acc(pred):5.1f}%  {name}")
        per = []
        for p in projects:
            sub = [r for r in single if r["project"] == p]
            if sub:
                per.append(
                    f"{p[:10]} {100 * sum(1 for r in sub if pred(r) == r['form']) / len(sub):.0f}%"
                )
        out.append("          per project: " + ", ".join(per))
    out.append(
        f"  threshold scan for 'one-line if L1 <= T else two-liner': "
        + ", ".join(
            f"T={T}:{100 * acc(lambda r, T=T: 'one-line' if r['L1'] <= T else 'two-liner'):.1f}%"
            for T in (60, 70, 80, 90, 100, 110, 120, 130, 140)
        )
    )
    # restricted to the choice one-line vs two-liner
    two = [r for r in single if r["form"] in ("one-line", "two-liner")]
    if two:
        best = (-1.0, 0)
        for T in range(40, 161, 2):
            a = sum(
                1
                for r in two
                if ("one-line" if r["L1"] <= T else "two-liner") == r["form"]
            ) / len(two)
            if a > best[0]:
                best = (a, T)
        out.append(
            f"  restricted to bodies written one-line or two-liner ({len(two)}): best threshold T={best[1]} gives {100 * best[0]:.1f}%"
        )

    # chain consistency: does an else / else-if branch use the same form as the branch before it?
    by_close = {(r["fid"], r["close"]): r for r in rows}
    nxt = {}
    pair = collections.Counter()

    def simple(r):
        return r["stmts"] == 1 and not r["nested"] and not r["comment"] and not r["multi_head"]

    for r in rows:
        if r["kind"] in ("else", "else-if"):
            p = by_close.get((r["fid"], r["hf"] - 1))
            if p is not None:
                nxt[id(p)] = r
                if simple(r) and r["L1"] <= 134:
                    pair[(p["form"], r["form"])] += 1
    out.append("")
    out.append("if/else chains: form of a single-statement else / else-if branch (L1 <= 134) given the form of the branch before it:")
    for (a, b), v in sorted(pair.items()):
        out.append(f"  previous branch {a:18s} -> this branch {b:10s} {v:6d}")

    def chain_acc(mode, T):
        ok = tot = 0
        for r in rows:
            if r["kind"] not in ("if", "for", "while"):
                continue
            ch = [r]
            while id(ch[-1]) in nxt:
                ch.append(nxt[id(ch[-1])])
            allone = all(simple(b) and b["L1"] <= T for b in ch)
            prevform = None
            for b in ch:
                if not simple(b):
                    prevform = "expanded"
                    continue
                if mode == "uniform":
                    pred = "one-line" if allone else ("two-liner" if b["L2"] <= 140 else "expanded")
                else:
                    pred = "one-line" if (b["L1"] <= T and prevform in (None, "one-line")) else ("two-liner" if b["L2"] <= 140 else "expanded")
                prevform = pred
                if b["form"] != "attached-expanded":
                    tot += 1
                    ok += pred == b["form"]
        return 100.0 * ok / max(1, tot)

    out.append(f"  rule 'whole chain one-line only if every branch fits (T=134), else two-liners': {chain_acc('uniform', 134):.1f}%  (worse than the plain threshold)")
    out.append(f"  rule 'branch is one-line only if it fits and the branch before it is one-line': {chain_acc('seq', 134):.1f}%  (worse than the plain threshold)")
    out.append("")
    # multi-statement bodies: one-line when short?
    out.append(
        "multi-statement bodies without nested blocks or comments: form by L1 bucket"
    )
    for s in (2, 3):
        sub = [
            r
            for r in rows
            if r["stmts"] == s
            and not r["nested"]
            and not r["comment"]
            and not r["multi_head"]
        ]
        out.append(f"  {s} statements ({len(sub)} bodies)")
        bk = collections.Counter(
            ((min(r["L1"], 179) // 20) * 20, r["form"]) for r in sub
        )
        for b in range(0, 180, 20):
            if any(bk[(b, fm)] for fm in forms):
                out.append(
                    f"    L1 {b:3d}..{b + 19 if b < 160 else 999:<4d}"
                    + "".join(f"{fm}={bk[(b, fm)]:<6d} " for fm in forms[:3])
                )
        best = (-1.0, 0)
        for T in range(30, 161, 2):
            a = sum(
                1
                for r in sub
                if ("one-line" if r["L1"] <= T else "expanded")
                == ("one-line" if r["form"] == "one-line" else "expanded")
            ) / max(1, len(sub))
            if a > best[0]:
                best = (a, T)
        base = sum(1 for r in sub if r["form"] != "one-line") / max(1, len(sub))
        out.append(
            f"    best 'join onto one line if L1 <= T' threshold: T={best[1]} accuracy {100 * best[0]:.1f}%; 'never join' accuracy {100 * base:.1f}%"
        )


BRACE_TABLES = [
    ("function", "function definitions (namespace / file scope)"),
    ("inline-member-function", "member functions defined inside a class body"),
    ("class", "class"),
    ("struct", "struct"),
    ("nested-class", "class / struct nested inside a class"),
    ("namespace", "namespace"),
    ("enum", "enum"),
    ("union", "union"),
    ("if", "if"),
    ("else-if", "else if"),
    ("else", "else"),
    ("for", "for"),
    ("while", "while"),
    ("do", "do-while"),
    ("switch", "switch"),
    ("case-block", "braced block after a case label"),
    ("try", "try"),
    ("catch", "catch"),
    ("bare-block", "bare scope block"),
    ("extern-block", 'extern "C" block'),
]


def fit_multi(rows, out):
    """Evidence for the rule the formatter applies to bodies of two and three statements."""
    out.append("")
    out.append("## Bodies of two and three statements: the rule the formatter applies")
    out.append("rule: on the statement / signature line if that line is at most 140 columns; else the braced body alone on the")
    out.append("next line if that fits; else expanded.  Population: no nested block, no comment, statement or signature on one line.")
    groups = (
        ("control statements (if / else / for / while)", ("if", "else-if", "else", "for", "while")),
        ("functions at namespace / file scope", ("function",)),
        ("member functions defined in a class body", ("inline-member-function",)),
    )
    for title, kinds in groups:
        # L2 assumes a body line indented one level; a function's body line is not indented
        body_offset = 0 if kinds[0] == "if" else 2
        for stmts in (2, 3):
            sub = [
                r
                for r in rows
                if r["kind"] in kinds and r["stmts"] == stmts and not r["nested"] and not r["comment"] and not r["multi_head"] and r["form"] != "attached-expanded"
            ]
            fits = [r for r in sub if r["L1"] <= 140]
            one = sum(1 for r in fits if r["form"] == "one-line")
            two = sum(1 for r in fits if r["form"] == "two-liner")

            def rule(r):
                if r["L1"] <= 140:
                    return "one-line"
                return "two-liner" if r["L2"] - body_offset <= 140 else "expanded"

            ok = sum(1 for r in sub if rule(r) == r["form"])
            never = sum(1 for r in sub if r["form"] == "expanded")
            out.append(
                f"  {title}, {stmts} statements: {len(sub)} bodies; {len(fits)} would fit on one line, of which written on one line {one}, "
                f"body alone on the next line {two}, expanded {len(fits) - one - two}; rule predicts {100.0 * ok / max(1, len(sub)):.1f}%, 'always expand' {100.0 * never / max(1, len(sub)):.1f}%"
            )


def main():
    ap = argparse.ArgumentParser(description="Survey of the layout habits in a hand-formatted C++ corpus.")
    common.add_corpus_arguments(ap)
    a = ap.parse_args()
    root, files = common.corpus_files(a)
    C = Counts()
    rows = []
    alive = []  # rows refer to their file by id(); the File objects must outlive the loop
    for rel in files:
        project = common.project_of(rel)
        f = File(common.read(os.path.join(root, rel)))
        alive.append(f)
        kind_of = survey_file(f, project, C, rows)
        survey_spacing(f, project, C)
        survey_lines(f, project, C, kind_of)
        C.add("files", project, "files")
    out = [
        f"# Corpus survey: {len(files)} files in {len({common.project_of(r) for r in files})} projects",
        "Columns are projects; 'own-line +N' is the brace column minus the indentation of the line the construct starts on.",
    ]
    out.append(C.report("files"))
    out.append(C.report("lines"))
    out.append("\n# A1. Brace placement and indentation per construct")
    for k, title in BRACE_TABLES:
        if ("brace:" + k) in C.c:
            out.append(C.report("brace:" + k, "braces: " + title))
            if ("bodyindent:" + k) in C.c:
                out.append(C.report("bodyindent:" + k, "body indentation: " + title))
    out.append(C.report("brace:lambda", "braces: lambda bodies", show_examples=True))
    out.append(C.report("brace:init-list", "braces: initializer lists"))
    out.append(C.report("do-while-tail", "do-while: placement of the trailing while"))
    out.append(
        "\n# A2. Indentation of namespace bodies, access specifiers, case labels"
    )
    for t in (
        "namespace-body-indent",
        "access-specifier-indent",
        "case-label-indent",
        "case-body",
    ):
        out.append(C.report(t))
    out.append("\n# A3. Constructor initializer lists")
    out.append(C.report("ctor-init"))
    out.append(C.report("ctor-init-indent"))
    out.append("\n# A4. Braces on control statements, else placement")
    out.append(C.report("braces-on-control"))
    out.append(C.report("else-placement"))
    fit_rule(rows, out)
    fit_functions(rows, out)
    fit_multi(rows, out)
    out.append("\n# A5. Pointers and references")
    for t in (
        "pointer:in-template-args",
        "pointer:in-c-cast",
        "pointer:declaration (variable, parameter, member)",
        "pointer:return-type-or-qualified-name",
    ):
        out.append(C.report(t))
    out.append("\n# A6. Spacing")
    for t in (
        "space:control-paren",
        "space:call-paren",
        "space:template",
        "template-line",
        "space:binary-operators",
        "space:after-comma",
        "space:before-comma",
        "space:inside-parens",
        "space:inside-parens-close",
        "space:after-c-cast",
        "space:init-braces",
        "space:before-init-brace",
        "space:empty-init-braces",
    ):
        out.append(C.report(t))
    out.append("\n# A7. Blank lines, trailing whitespace, line length")
    for t in (
        "blank-lines-after-function",
        "blank-line-runs",
        "trailing-whitespace",
        "line-length",
        "tabs",
        "indent-width",
    ):
        out.append(C.report(t))
    out.append("\n# A8. Includes")
    for t in ("includes:order", "includes:groups", "includes:sorted"):
        out.append(C.report(t))
    out.append("\n# A9. Hand alignment")
    for t in (
        "align:=",
        "align:= (runs)",
        "align:sign-offset",
        "align:declaration-names",
        "align:trailing-comments",
        "align:arguments",
        "align:any",
        "continuation",
    ):
        out.append(C.report(t))
    out.append("\n# A10. Empty function bodies")
    out.append(C.report("empty-function-body"))
    print("\n".join(out))


if __name__ == "__main__":
    main()
