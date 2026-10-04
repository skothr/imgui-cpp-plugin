#!/usr/bin/env python3
"""imtool_format.py: format C++ deterministically in one step.

    imtool_format.py [--check] [--no-brace-check] [--style-file F] [--clang-format BIN] files...

Step 1 runs clang-format with the style file (default: imtool.clang-format next to this script).
Step 2 is a post-pass for the parts of the style clang-format cannot express:

  braces      enum braces are indented one level, like control-statement braces (the style file
              indents the braces of if / for / while / do / switch / try / catch, GNU layout);
              a wrapped lambda brace sits at the column clang-format started the lambda at,
              not one level in; "} while(..);" closes a do body on one line
  bodies      a function body or an if / else / for / while body of one to three statements
              goes on the line of its signature or statement when the whole line fits;
              otherwise the braced body goes alone on the next line when that line fits;
              otherwise the block stays expanded.  A body with a comment, a preprocessor
              line, a nested block or a braceless control statement is never joined.
  pointers    no space before * and & that have no name after them: <T*>, (T*), f(int*, T&)
              a function's return type binds left: T* f(), T& C::g(), T& operator[](int)
  operators   binary * / % are written without spaces: a*b + c/d

Without --check the files are rewritten in place.  With --check nothing is written and the
files that would change are printed.

Control statements whose body has no braces are reported on stderr with file and line: as
errors with --check, as warnings otherwise.  The tool does not add the braces, because it never
changes tokens.  --no-brace-check turns the report off.

Exit status: 0 clean; 1 if --check found files to change, or any unbraced body was reported;
2 if a file could not be formatted, the clang-format binary is missing, or its version is not
the one pinned in the first line of the style file.

clang-format is looked up in this order: --clang-format, $IMTOOL_CLANG_FORMAT, the binary
installed by the pip package clang-format in the running Python environment, PATH.

The post-pass changes whitespace and line breaks only.  It never touches preprocessor lines,
the inside of string / character literals or line comments, or the lines between
"// clang-format off" and "// clang-format on".  Before writing, the tool compares the token
sequence of its output with the input and refuses to write on any difference.  It also refuses
a file on which clang-format plus the post-pass does not reach a fixed point.

Python 3 standard library only.
"""

import argparse
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import sysconfig

# --------------------------------------------------------------------------- lexer

ID_RE = re.compile(r"[A-Za-z_$][A-Za-z0-9_$]*")
NUM_RE = re.compile(r"\.?[0-9](?:[eEpP][+-]|[A-Za-z0-9_.]|'(?=[A-Za-z0-9_]))*")
STR_RE = re.compile(r'(?:u8|u|U|L)?(R)?"')
CHR_RE = re.compile(r"(?:u8|u|U|L)?'")
RAW_DELIM_RE = re.compile(r'[^()\\\s"]{0,16}\(')
OPS = sorted(
    [
        "<<=", ">>=", "<=>", "->*", "...", "::", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
        "&&", "||", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", ".*", "##",
    ],
    key=len,
    reverse=True,
)  # fmt: skip
CODE = ("id", "num", "op", "str", "chr")


def _line_comment_end(text, i):
    """End of a // comment starting at i (a backslash-newline continues it)."""
    n = len(text)
    j = i
    while True:
        k = text.find("\n", j)
        if k < 0:
            return n
        seg_end = k - 1 if k > 0 and text[k - 1] == "\r" else k
        if seg_end > i and text[seg_end - 1] == "\\":
            j = k + 1
            continue
        return seg_end


def lex(text):
    """Split text into (kind, text) tokens whose concatenation equals text.

    kinds: ws nl lc (line comment) bc (block comment) pp (a whole preprocessor directive,
    continuation lines included) str chr id num op
    """
    toks = []
    i, n = 0, len(text)
    line_start = True
    while i < n:
        c = text[i]
        if c == "\n":
            toks.append(("nl", c))
            i += 1
            line_start = True
            continue
        if c == "\r" and text.startswith("\r\n", i):
            toks.append(("nl", "\r\n"))
            i += 2
            line_start = True
            continue
        if c in " \t\f\v\r":
            j = i + 1
            while j < n and (
                text[j] in " \t\f\v"
                or (text[j] == "\r" and not text.startswith("\r\n", j))
            ):
                j += 1
            toks.append(("ws", text[i:j]))
            i = j
            continue
        if c == "/" and text.startswith("//", i):
            j = _line_comment_end(text, i)
            toks.append(("lc", text[i:j]))
            i = j
            line_start = False
            continue
        if c == "/" and text.startswith("/*", i):
            k = text.find("*/", i + 2)
            j = n if k < 0 else k + 2
            toks.append(("bc", text[i:j]))
            i = j
            line_start = False
            continue
        if c == "#" and line_start:
            j = i + 1
            while j < n:
                d = text[j]
                if d == "\n" or (d == "\r" and text.startswith("\r\n", j)):
                    break
                if d == "\\" and (
                    text.startswith("\\\n", j) or text.startswith("\\\r\n", j)
                ):
                    j += 2 if text[j + 1] == "\n" else 3
                    continue
                if d in "\"'":
                    # a comment marker inside a literal is not a comment
                    j = _directive_literal_end(text, i, j)
                    continue
                if d == "/" and text.startswith("/*", j):
                    k = text.find("*/", j + 2)
                    j = n if k < 0 else k + 2
                    continue
                if d == "/" and text.startswith("//", j):
                    j = _line_comment_end(text, j)
                    break
                j += 1
            toks.append(("pp", text[i:j]))
            i = j
            line_start = False
            continue
        line_start = False
        m = STR_RE.match(text, i)
        if m:
            if m.group(1):
                d = RAW_DELIM_RE.match(text, m.end())
                if d:
                    delim = text[m.end() : d.end() - 1]
                    k = text.find(")" + delim + '"', d.end())
                    j = n if k < 0 else k + len(delim) + 2
                    toks.append(("str", text[i:j]))
                    i = j
                    continue
            j = _quoted_end(text, m.end(), '"')
            toks.append(("str", text[i:j]))
            i = j
            continue
        m = CHR_RE.match(text, i)
        if m:
            j = _quoted_end(text, m.end(), "'")
            toks.append(("chr", text[i:j]))
            i = j
            continue
        m = ID_RE.match(text, i)
        if m:
            toks.append(("id", m.group()))
            i = m.end()
            continue
        m = NUM_RE.match(text, i)
        if m:
            toks.append(("num", m.group()))
            i = m.end()
            continue
        for op in OPS:
            if text.startswith(op, i):
                toks.append(("op", op))
                i += len(op)
                break
        else:
            toks.append(("op", c))
            i += 1
    return toks


_RAW_PREFIXES = ("R", "u8R", "uR", "UR", "LR")


def _directive_literal_end(text, start, j):
    """End of the string or character literal that opens at text[j] in the directive at `start`.

    The literal ends after its closing quote, or at the end of the line when it has none
    (the apostrophe in '#error don't').  A backslash-newline inside it continues the line.
    """
    n = len(text)
    quote = text[j]
    k = j
    while k > start and (text[k - 1].isalnum() or text[k - 1] in "_.'"):
        k -= 1
    prefix = text[k:j]
    if quote == "'" and prefix[:1].isdigit():
        return j + 1  # digit separator: 1'000
    if quote == '"' and prefix in _RAW_PREFIXES:
        d = RAW_DELIM_RE.match(text, j + 1)
        if d:
            delim = text[j + 1 : d.end() - 1]
            e = text.find(")" + delim + '"', d.end())
            return n if e < 0 else e + len(delim) + 2
    k = j + 1
    while k < n:
        ch = text[k]
        if ch == "\\":
            k += 3 if text.startswith("\\\r\n", k) else 2
            continue
        if ch == quote:
            return k + 1
        if ch == "\n" or (ch == "\r" and text.startswith("\r\n", k)):
            return k
        k += 1
    return n


def _quoted_end(text, j, quote):
    n = len(text)
    while j < n:
        d = text[j]
        if d == "\\":
            j += 2
            continue
        if d == quote:
            return j + 1
        if d == "\n":
            return j
        j += 1
    return n


_WS_RE = re.compile(r"\s+")


def token_signature(text):
    """What must not change: the token sequence, ignoring whitespace.

    Adjacent operator tokens are concatenated ('> >' and '>>' compare equal, as they do with all
    whitespace removed); identifiers, numbers and literals are compared one by one, so two words
    merging into one is a difference.  Comments and preprocessor directives are compared with
    their whitespace removed.
    """
    sig = []
    ops = []
    for kind, tok in lex(text):
        if kind in ("ws", "nl"):
            continue
        if kind == "op":
            ops.append(tok)
            continue
        if ops:
            sig.append(("op", "".join(ops)))
            ops = []
        if kind in ("lc", "bc", "pp"):
            sig.append((kind, _WS_RE.sub("", tok)))
        else:
            sig.append((kind, tok))
    if ops:
        sig.append(("op", "".join(ops)))
    return sig


# --------------------------------------------------------------------------- line model


class Line:
    """One logical line: leading indentation, tokens (no leading whitespace, no newline), newline.

    A token that spans physical lines (block comment, raw string, continued preprocessor
    directive) stays inside one Line, so indentation changes never reach into it.
    """

    __slots__ = ("indent", "toks", "nl", "protected")

    def __init__(self, indent, toks, nl):
        self.indent, self.toks, self.nl, self.protected = indent, toks, nl, False

    def code(self):
        return [t for t in self.toks if t[0] in CODE]

    def text(self):
        return "".join(t[1] for t in self.toks).rstrip(" \t")

    def has_comment(self):
        return any(t[0] in ("lc", "bc") for t in self.toks)

    def is_pp(self):
        return any(t[0] == "pp" for t in self.toks)

    def multiline(self):
        return any("\n" in t[1] for t in self.toks)


def split_lines(text):
    lines = []
    cur = []
    for tok in lex(text):
        if tok[0] == "nl":
            lines.append(_make_line(cur, tok[1]))
            cur = []
        else:
            cur.append(tok)
    if cur:
        lines.append(_make_line(cur, ""))
    return lines


def _make_line(toks, nl):
    if toks and toks[0][0] == "ws":
        return Line(toks[0][1], toks[1:], nl)
    return Line("", toks, nl)


def join_lines(lines):
    return "".join(ln.indent + "".join(t[1] for t in ln.toks) + ln.nl for ln in lines)


_OFF_RE = re.compile(r"^(?://\s*clang-format\s+off\b|/\*\s*clang-format\s+off\s*\*/$)")
_ON_RE = re.compile(r"^(?://\s*clang-format\s+on\b|/\*\s*clang-format\s+on\s*\*/$)")


def mark_protected(lines):
    """Lines from a 'clang-format off' comment through the matching 'clang-format on' comment."""
    off = False
    for ln in lines:
        prot = off
        for kind, tok in ln.toks:
            if kind in ("lc", "bc"):
                if _OFF_RE.match(tok):
                    off = True
                    prot = True
                elif _ON_RE.match(tok):
                    off = False
                    prot = True
        ln.protected = prot


class Structure:
    """Code tokens of all non-preprocessor lines with their positions and bracket matching."""

    def __init__(self, lines):
        self.sig = []  # (line index, index in line.code(), kind, text)
        self.first = {}  # line index -> sig index of its first code token
        for li, ln in enumerate(lines):
            if ln.is_pp():
                continue
            for ci, (kind, tok) in enumerate(ln.code()):
                if ci == 0:
                    self.first[li] = len(self.sig)
                self.sig.append((li, ci, kind, tok))
        self.match = {}
        stack = []
        pairs = {")": "(", "]": "[", "}": "{"}
        for i, (_, _, kind, tok) in enumerate(self.sig):
            if kind != "op":
                continue
            if tok in ("(", "[", "{"):
                stack.append(i)
            elif tok in pairs:
                while stack and self.sig[stack[-1]][3] != pairs[tok]:
                    stack.pop()
                if stack:
                    j = stack.pop()
                    self.match[i] = j
                    self.match[j] = i


# --------------------------------------------------------------------------- brace indentation


def _enum_head(S, i):
    """Line index of the 'enum' keyword if sig[i] ('{') opens an enum body, else None."""
    k = i - 1
    steps = 0
    while k >= 0 and steps < 24:
        tok = S.sig[k][3]
        if S.sig[k][2] == "op" and tok in (";", "{", "}", "(", ")", "="):
            return None
        if S.sig[k][2] == "id" and tok == "enum":
            return S.sig[k][0]
        k -= 1
        steps += 1
    return None


def _attribute_open(S, k):
    """sig index of the outer '[' if sig[k] is the outer ']' of a '[[...]]' attribute, else None."""
    if k < 3 or S.sig[k][2:] != ("op", "]") or S.sig[k - 1][2:] != ("op", "]"):
        return None
    o = S.match.get(k)
    if o is None or S.sig[o + 1][2:] != ("op", "[") or S.match.get(o + 1) != k - 1:
        return None
    return o


def _lambda_brace(S, i):
    """True if sig[i] ('{') opens a lambda body: it follows '[captures](params)' or '[captures]'."""
    k = i - 1
    while k >= 0:
        if S.sig[k][2] == "id" and S.sig[k][3] in ("mutable", "noexcept", "constexpr"):
            k -= 1
            continue
        # a '[[...]]' attribute before the brace is not a capture list: 'case 1: [[likely]] {'
        o = _attribute_open(S, k)
        if o is None:
            break
        k = o - 1
    if k < 0 or S.sig[k][2] != "op":
        return False
    if S.sig[k][3] == ")":
        o = S.match.get(k)
        if o is None or o == 0:
            return False
        k = o - 1
    if S.sig[k][3] != "]" or S.sig[k][2] != "op":
        return False
    o = S.match.get(k)
    if o is None:
        return False
    if o == 0:
        return True
    before = S.sig[o - 1]
    # '[' after a name, ')' or ']' is a subscript, not a lambda introducer
    if before[2] in ("num", "str", "chr") or (
        before[2] == "id" and before[3] not in ("return", "co_return")
    ):
        return False
    return not (before[2] == "op" and before[3] in (")", "]"))


def join_do_while(lines, S, limit):
    """'}' ending a do body and the 'while(..);' on the next line are joined: '} while(..);'."""
    drop = set()
    for i, (_li, _ci, kind, tok) in enumerate(S.sig):
        if (
            kind != "op"
            or tok != "{"
            or i == 0
            or S.sig[i - 1][2] != "id"
            or S.sig[i - 1][3] != "do"
        ):
            continue
        j = S.match.get(i)
        if j is None or j + 1 >= len(S.sig):
            continue
        lj = S.sig[j][0]
        cl = lines[lj]
        nli, nci, nkind, ntok = S.sig[j + 1]
        if nkind != "id" or ntok != "while" or nli != lj + 1 or nci != 0:
            continue
        wl = lines[nli]
        if cl.protected or wl.protected or wl.is_pp() or cl.multiline():
            continue
        code = cl.code()
        if S.sig[j][1] != len(code) - 1 or cl.has_comment():
            continue  # something follows the brace on its line
        if len(cl.indent) + len(cl.text()) + 1 + len(wl.text()) > limit:
            continue
        cl.toks = _strip_ws(cl.toks) + [("ws", " ")] + wl.toks
        cl.nl = wl.nl
        drop.add(nli)
    if not drop:
        return lines
    return [ln for x, ln in enumerate(lines) if x not in drop]


def find_shift_blocks(lines, S, indent_width):
    """Blocks whose brace lines and body move: [(open line, close line, delta)]."""
    blocks = []
    for i, (li, _ci, kind, tok) in enumerate(S.sig):
        if kind != "op" or tok != "{" or i == 0:
            continue
        ln = lines[li]
        if len(ln.code()) != 1:
            continue  # only braces alone on their line
        j = S.match.get(i)
        if j is None:
            continue
        lj = S.sig[j][0]
        if lj <= li or S.sig[j][1] != 0 or len(lines[lj].indent) != len(ln.indent):
            continue
        if ln.protected or lines[lj].protected:
            continue
        if _lambda_brace(S, i):
            # clang-format (23) indents a wrapped lambda brace one level; the style wants it
            # at the column the lambda starts at, or at the statement's indentation
            if len(ln.indent) >= indent_width:
                blocks.append((li, lj, -indent_width))
            continue
        head = _enum_head(S, i)
        if head is None or lines[head].protected:
            continue
        if len(ln.indent) == len(lines[head].indent):
            blocks.append((li, lj, indent_width))
    return blocks


def _comment_room(tok):
    """Smallest leading-space count over the non-blank continuation lines of a block comment."""
    room = None
    for cont in tok.split("\n")[1:]:
        if cont.strip():
            lead = len(cont) - len(cont.lstrip(" "))
            room = lead if room is None else min(room, lead)
    return room


def _shift_comment(tok, delta):
    parts = tok.split("\n")
    out = [parts[0]]
    for cont in parts[1:]:
        if not cont.strip():
            out.append(cont)
        elif delta > 0:
            out.append(" " * delta + cont)
        else:
            out.append(cont[-delta:])
    return "\n".join(out)


def _pinned_comments(lines):
    """Comment-only lines that continue the trailing comment of the line above.

    clang-format keeps such a comment at its column instead of indenting it with the code
    (AlignTrailingComments: Leave), so the post-pass must not move it either; moving it would
    add the shift again on every run.  A comment at the indentation of the next code line is an
    ordinary comment and moves with the code.
    """
    pinned = set()
    prev_trailing = False
    for x, ln in enumerate(lines):
        kinds = [k for k, _ in ln.toks if k != "ws"]
        if kinds and all(k == "lc" for k in kinds):
            if prev_trailing:
                nxt = next((l for l in lines[x + 1 :] if l.code()), None)
                if nxt is None or len(nxt.indent) != len(ln.indent):
                    pinned.add(x)
                    continue
            prev_trailing = False
        else:
            prev_trailing = bool(ln.code()) and kinds[-1] == "lc"
    return pinned


def apply_shifts(lines, blocks):
    """Shift the lines of each block.  A block is dropped when a line in it cannot move (too
    little indentation, or a block comment continuation line with too little indentation)."""
    pinned = _pinned_comments(lines)
    comment_only = {
        x
        for x, ln in enumerate(lines)
        if ln.toks and all(k in ("lc", "ws") for k, _ in ln.toks)
    }
    while True:
        delta = [0] * len(lines)
        for a, b, d in blocks:
            for x in range(a, b + 1):
                # d > 0 is an enum body: clang-format keeps a comment line there at the column
                # it was written at, so it must stay put here as well
                if x not in pinned and not (d > 0 and x in comment_only):
                    delta[x] += d
        bad = None
        for x, ln in enumerate(lines):
            d = delta[x]
            if d >= 0 or ln.protected or ln.is_pp() or not ln.toks:
                continue
            if len(ln.indent) < -d or "\t" in ln.indent:
                bad = x
                break
            for kind, tok in ln.toks:
                if kind == "bc" and "\n" in tok:
                    room = _comment_room(tok)
                    if room is not None and room < -d:
                        bad = x
            if bad is not None:
                break
        if bad is None:
            break
        blocks = [bl for bl in blocks if not (bl[0] <= bad <= bl[1] and bl[2] < 0)]
    for x, ln in enumerate(lines):
        d = delta[x]
        if d == 0 or ln.protected or ln.is_pp() or not ln.toks:
            continue
        ln.indent = " " * (len(ln.indent) + d)
        if any(kind == "bc" and "\n" in tok for kind, tok in ln.toks):
            ln.toks = [
                (kind, _shift_comment(tok, d))
                if kind == "bc" and "\n" in tok
                else (kind, tok)
                for kind, tok in ln.toks
            ]


# --------------------------------------------------------------------------- token spacing

PTR = ("*", "&", "&&")
NOT_A_TYPE = {
    "return", "else", "case", "throw", "delete", "new", "co_return", "co_yield", "co_await", "sizeof", "typeid",
    "goto", "if", "for", "while", "switch", "do", "using", "typedef", "and", "or", "not", "alignof", "decltype",
}  # fmt: skip


def tight_unnamed_declarators(toks):
    """'T *>' -> 'T*>', '(T *)' -> '(T*)', 'f(int *, T &)' -> 'f(int*, T&)'.

    A run of * & && that is followed by > >> , or ) declares nothing by name; the space before
    it is removed.  Such a run cannot be a binary operator (its right operand would be missing).
    """
    n = len(toks)
    out = []
    i = 0
    while i < n:
        kind = toks[i][0]
        if (
            kind == "ws"
            and 0 < i < n - 1
            and toks[i + 1][0] == "op"
            and toks[i + 1][1] in PTR
        ):
            prev = toks[i - 1]
            j = i + 1
            while j < n and toks[j][0] == "op" and toks[j][1] in PTR:
                j += 1
            nxt = toks[j] if j < n else None
            prev_ok = (prev[0] == "id" and prev[1] not in NOT_A_TYPE) or prev == (
                "op",
                ">",
            )
            if (
                nxt is not None
                and nxt[0] == "op"
                and nxt[1] in (">", ">>", ",", ")")
                and prev_ok
            ):
                i += 1
                continue
        out.append(toks[i])
        i += 1
    return out


def bind_return_type_left(toks):
    """'T *f(' -> 'T* f(', 'T &C::g(' -> 'T& C::g(', 'T &operator[](' -> 'T& operator[]('.

    Only for a declaration that starts the line: everything before the * or & must spell a type
    (names, ::, template arguments), and the name after it must be followed by '('.  The
    whitespace run moves from before the * to after it, so the line keeps its width.
    """
    n = len(toks)
    depth = 0
    i = 0
    seen_type = False
    # leading attributes: [[nodiscard]] T *f()
    while i + 1 < n and toks[i] == ("op", "[") and toks[i + 1] == ("op", "["):
        j = i + 2
        while j + 1 < n and not (toks[j] == ("op", "]") and toks[j + 1] == ("op", "]")):
            j += 1
        if j + 1 >= n:
            return toks
        i = j + 2
        while i < n and toks[i][0] == "ws":
            i += 1
    while i < n:
        kind, tok = toks[i]
        if kind == "ws":
            i += 1
            continue
        if kind == "id":
            if tok in NOT_A_TYPE or tok == "operator":
                return toks
            seen_type = True
        elif kind == "num" and depth > 0:
            pass
        elif kind == "op" and tok == "<":
            depth += 1
        elif kind == "op" and tok in (">", ">>"):
            depth -= 1 if tok == ">" else 2
            if depth < 0:
                return toks
        elif kind == "op" and tok in ("::", ","):
            if tok == "," and depth == 0:
                return toks
        elif kind == "op" and tok in PTR:
            if depth == 0:
                break
        elif kind == "op" and tok in ("(", ")", "[", "]") and depth > 0:
            pass
        else:
            return toks
        i += 1
    else:
        return toks
    if not seen_type or i < 2 or toks[i - 1][0] != "ws":
        return toks
    prev = toks[i - 2]
    if not ((prev[0] == "id" and prev[1] not in NOT_A_TYPE) or prev == ("op", ">")):
        return toks
    j = i
    while j < n and toks[j][0] == "op" and toks[j][1] in PTR:
        j += 1
    if j >= n or toks[j][0] != "id" or toks[j][1] in NOT_A_TYPE:
        return toks
    # the declared name: id (:: id | <...>)* then '(' ; or anything spelled with 'operator'
    k = j
    depth = 0
    is_operator = False
    while k < n:
        kind, tok = toks[k]
        if kind == "id" and tok == "operator" and depth == 0:
            is_operator = True
            break
        if (
            kind == "id"
            or (kind == "op" and tok in ("::", "~"))
            or (
                depth > 0
                and (kind in ("num", "ws") or (kind == "op" and tok in (",", "*", "&")))
            )
        ):
            pass
        elif kind == "op" and tok == "<":
            depth += 1
        elif kind == "op" and tok == ">" and depth > 0:
            depth -= 1
        else:
            break
        k += 1
    if is_operator:
        if not any(t == ("op", "(") for t in toks[k:]):
            return toks
    elif k >= n or toks[k] != ("op", "(") or depth != 0:
        return toks
    return toks[: i - 1] + toks[i:j] + [toks[i - 1]] + toks[j:]


def tight_muldiv(toks):
    """'a * b' -> 'a*b', 'a / b' -> 'a/b', 'a % b' -> 'a%b' for binary operators.

    clang-format writes a binary operator with one space on each side and a declarator with a
    space on one side only, so only the first form is touched.  Joins that would create another
    token ('/' before '*' or '/', '%' before ':') are left alone.
    """
    n = len(toks)
    out = []
    i = 0
    while i < n:
        if (
            toks[i] == ("ws", " ")
            and 0 < i
            and i + 3 < n
            and toks[i + 1][0] == "op"
            and toks[i + 1][1] in ("*", "/", "%")
            and toks[i + 2] == ("ws", " ")
        ):
            prev, op, nxt = toks[i - 1], toks[i + 1][1], toks[i + 3]
            left = (
                (prev[0] == "id" and prev[1] not in NOT_A_TYPE)
                or prev[0] == "num"
                or (prev[0] == "op" and prev[1] in (")", "]"))
            )
            right = nxt[0] in ("id", "num") or (
                nxt[0] == "op" and nxt[1] in ("(", "-", "+", "!", "~", "::")
            )
            unsafe = (
                (op == "/" and nxt[1][:1] in ("*", "/"))
                or (op == "%" and nxt[1][:1] in (":", ">"))
                or (op == "*" and prev[1][-1:] == "/")
            )
            if left and right and not unsafe:
                out.append(toks[i + 1])
                i += 3
                continue
        out.append(toks[i])
        i += 1
    return out


# --------------------------------------------------------------------------- body layout

CONTROL_START = {
    "if",
    "for",
    "while",
    "do",
    "switch",
    "case",
    "default",
    "else",
    "try",
    "catch",
}


def _simple_statement(ln, indent):
    """A complete statement on one physical line at the given indentation, ending in ';'."""
    if (
        ln.protected
        or ln.is_pp()
        or ln.has_comment()
        or ln.multiline()
        or len(ln.indent) != indent
    ):
        return False
    code = ln.code()
    if not code or code[-1] != ("op", ";") or code[0][1] in CONTROL_START:
        return False
    depth = 0
    for kind, tok in code:
        if kind != "op":
            continue
        if tok in ("{", "}"):
            return False
        if tok in ("(", "["):
            depth += 1
        elif tok in (")", "]"):
            depth -= 1
            if depth < 0:
                return False
    return depth == 0


def _strip_ws(toks):
    toks = list(toks)
    while toks and toks[-1][0] == "ws":
        toks.pop()
    return toks


def _control_keyword(S, i):
    """sig index of the keyword that starts the if / else if / else / for / while statement whose
    body opens at sig[i] ('{'), or None."""
    _pli, _pci, pkind, ptok = S.sig[i - 1]
    if pkind == "id" and ptok == "else":
        return i - 1
    if pkind == "op" and ptok == ")":
        o = S.match.get(i - 1)
        if (
            o is not None
            and o > 0
            and S.sig[o - 1][2] == "id"
            and S.sig[o - 1][3] in ("if", "for", "while")
        ):
            kw = o - 1
            if (
                kw > 0
                and S.sig[kw - 1][3] == "else"
                and S.sig[kw - 1][0] == S.sig[kw][0]
            ):
                kw -= 1
            return kw
    return None


def _declaration_start(S, i):
    """sig index of the first token of the declaration whose body opens at sig[i], after any
    leading 'template<...>' and access specifier."""
    depth = 0
    k = i - 1
    while k >= 0:
        kind, tok = S.sig[k][2], S.sig[k][3]
        if kind == "op":
            if tok in (")", "]"):
                depth += 1
            elif tok in ("(", "["):
                if depth == 0:
                    break
                depth -= 1
            elif depth == 0 and tok in (";", "{", "}"):
                break
            elif (
                depth == 0
                and tok == ":"
                and k > 0
                and S.sig[k - 1][3] in ("public", "private", "protected")
            ):
                break
        k -= 1
    start = k + 1
    while start < i and S.sig[start][3] == "template" and S.sig[start + 1][3] == "<":
        depth = 0
        m = start + 1
        while m < i:
            tok = S.sig[m][3]
            if tok == "<":
                depth += 1
            elif tok == ">":
                depth -= 1
            elif tok == ">>":
                depth -= 2
            if depth <= 0:
                break
            m += 1
        start = m + 1
    return start


def join_bodies(lines, limit, max_statements, indent_width):
    """Layout of short function bodies and if / else / for / while bodies.

    A body of 1..max_statements statements goes on the line of its statement or signature when
    the whole line fits in `limit` columns.  When it does not fit, or the statement or signature
    spans several lines, the braced body goes alone on the next line if that line fits.
    Otherwise the block stays expanded.  A body with a comment, a preprocessor line, a nested
    block or a braceless control statement is never joined.  Returns a new list of lines.
    """
    S = Structure(lines)
    edits = {}  # first replaced line -> (last replaced line, [new lines])
    for i, (li, ci, kind, tok) in enumerate(S.sig):
        if kind != "op" or tok != "{" or i == 0 or ci != 0:
            continue
        ln = lines[li]
        if ln.protected or ln.is_pp() or ln.multiline():
            continue
        j = S.match.get(i)
        if j is None:
            continue
        lj = S.sig[j][0]
        code = ln.code()
        if lj == li:
            # the braced body is already on one line of its own: '{ }' or '{ a; b; }'
            if S.sig[j][1] != len(code) - 1:
                continue
            before = [
                t
                for t in ln.toks[: _tok_index(ln, len(code) - 1)]
                if t[0] in ("lc", "bc")
            ]
            if before:
                continue
            packed = list(ln.toks)
            body = []
        else:
            if len(code) != 1 or ln.has_comment():
                continue
            nbody = lj - li - 1
            if nbody < 1 or nbody > max_statements:
                continue
            cl = lines[lj]
            if (
                cl.code() != [("op", "}")]
                or cl.has_comment()
                or cl.protected
                or len(cl.indent) != len(ln.indent)
            ):
                continue
            body = lines[li + 1 : lj]
            if not all(
                _simple_statement(b, len(ln.indent) + indent_width) for b in body
            ):
                continue
            packed = [("op", "{")]
            for b in body:
                packed += [("ws", " ")] + _strip_ws(b.toks)
            packed += [("ws", " "), ("op", "}")]
        if S.sig[i - 1][0] != li - 1:
            continue  # the statement or signature must end on the line just above the brace
        above = lines[li - 1]
        if above.protected or above.is_pp():
            continue
        kw = _control_keyword(S, i)
        if kw is not None:
            start = kw
            if len(ln.indent) != len(lines[S.sig[kw][0]].indent) + indent_width:
                continue
        elif _function_signature_end(S, i):
            start = _declaration_start(S, i)
            if len(ln.indent) != len(lines[S.sig[start][0]].indent):
                continue
        else:
            continue
        single_head = (
            S.sig[start][0] == li - 1
            and S.first.get(li - 1) == start
            and not above.has_comment()
            and not above.multiline()
        )
        packed_len = len("".join(t[1] for t in packed).rstrip(" \t"))
        one_len = len(above.indent) + len(above.text()) + 1 + packed_len
        nl = lines[lj].nl
        if single_head and one_len <= limit:
            new = Line(above.indent, _strip_ws(above.toks) + [("ws", " ")] + packed, nl)
            edits[li - 1] = (lj, [new])
        elif lj != li and len(ln.indent) + packed_len <= limit:
            edits[li] = (lj, [Line(ln.indent, packed, nl)])
    if not edits:
        return lines
    out = []
    x = 0
    while x < len(lines):
        if x in edits:
            last, new = edits[x]
            out += new
            x = last + 1
        else:
            out.append(lines[x])
            x += 1
    return out


def _tok_index(ln, code_index):
    """Index in ln.toks of the code token number code_index."""
    seen = -1
    for x, t in enumerate(ln.toks):
        if t[0] in CODE:
            seen += 1
            if seen == code_index:
                return x
    return len(ln.toks)


def _function_signature_end(S, i):
    """True if the '{' at sig[i] opens a function body: it follows ')' plus optional qualifiers,
    or a constructor initializer list, and the '(' is not that of a control statement or lambda."""
    k = i - 1
    quals = {"const", "noexcept", "override", "final", "volatile", "mutable"}
    while k >= 0 and S.sig[k][2] == "id" and S.sig[k][3] in quals:
        k -= 1
    if k < 0 or S.sig[k][3] != ")":
        return False
    o = S.match.get(k)
    if o is None or o == 0:
        return False
    before = S.sig[o - 1]
    is_operator = any(S.sig[x][3] == "operator" for x in range(max(0, o - 3), o))
    if not is_operator:
        if before[2] != "id" or before[3] in ("if", "for", "while", "switch", "catch"):
            return False
    # walk back to the start of the declaration: no '=' , 'return' or unbalanced bracket on the way
    depth = 0
    k = o - 1
    while k >= 0:
        kind, tok = S.sig[k][2], S.sig[k][3]
        if kind == "op":
            if tok in (")", "]"):
                depth += 1
            elif tok in ("(", "["):
                if depth == 0:
                    return False
                depth -= 1
            elif depth == 0 and tok in (";", "{", "}"):
                return True
        elif (
            kind == "id"
            and depth == 0
            and tok
            in (
                "return",
                "new",
                "throw",
                "case",
                "namespace",
                "class",
                "struct",
                "union",
                "enum",
            )
        ):
            return False
        k -= 1
    return True


# --------------------------------------------------------------------------- post-pass


def post_pass(
    text, limit=140, max_statements=3, indent_width=2, tight_ops=True, features=None
):
    """Whitespace-only transforms on clang-format output.  See the module docstring.

    features: optional set limiting the transforms, from
    {"braces", "pointers", "operators", "bodies"}; None enables all (operators only if tight_ops).
    """
    on = (
        features
        if features is not None
        else {"braces", "pointers", "operators", "bodies"}
    )
    lines = split_lines(text)
    mark_protected(lines)
    if "braces" in on:
        S = Structure(lines)
        apply_shifts(lines, find_shift_blocks(lines, S, indent_width))
        lines = join_do_while(lines, S, limit)
    for ln in lines:
        if ln.protected or ln.is_pp() or not ln.toks:
            continue
        if "pointers" in on:
            ln.toks = tight_unnamed_declarators(ln.toks)
            ln.toks = bind_return_type_left(ln.toks)
        if "operators" in on and tight_ops:
            ln.toks = tight_muldiv(ln.toks)
    if "bodies" in on:
        lines = join_bodies(lines, limit, max_statements, indent_width)
    return join_lines(lines)


# --------------------------------------------------------------------------- brace check


def unbraced_bodies(text):
    """Control statements whose body has no braces: [(line number, first line of the statement)].

    Checked: if, else, for, while, do.  'else if' counts as braced when the if is.  The
    'while(..);' that ends a do statement is not a finding.  Preprocessor lines, comments and
    literals are not scanned.
    """
    sig = []  # (line, kind, text)
    line = 1
    for kind, tok in lex(text):
        if kind in CODE:
            sig.append((line, kind, tok))
        line += tok.count("\n")
    match = {}
    stack = []
    pairs = {")": "(", "]": "[", "}": "{"}
    for i, (_, kind, tok) in enumerate(sig):
        if kind != "op":
            continue
        if tok in ("(", "[", "{"):
            stack.append(i)
        elif tok in pairs:
            while stack and sig[stack[-1]][2] != pairs[tok]:
                stack.pop()
            if stack:
                o = stack.pop()
                match[i] = o
                match[o] = i

    def skip_attributes(k):
        """Index after the '[[...]]' attributes that start at sig[k]: 'if(x) [[likely]] {'."""
        while (
            k + 1 < len(sig)
            and sig[k][1:] == ("op", "[")
            and sig[k + 1][1:] == ("op", "[")
        ):
            c = match.get(k)
            if c is None or match.get(k + 1) != c - 1:
                break
            k = c + 1
        return k

    src = text.split("\n")
    found = []
    do_tails = set()
    pending_do = 0  # unbraced 'do' statements whose 'while' has not been seen yet
    for i, (ln, kind, tok) in enumerate(sig):
        if kind != "id" or tok not in ("if", "else", "for", "while", "do"):
            continue
        b = skip_attributes(i + 1)  # where the body of an else / do starts
        nxt = sig[b] if b < len(sig) else None
        if nxt is None:
            continue
        if tok == "else":
            if nxt[2] not in ("{", "if"):
                found.append(ln)
            continue
        if tok == "do":
            if nxt[2] != "{":
                found.append(ln)
                pending_do += 1
            else:
                c = match.get(b)
                if c is not None and c + 1 < len(sig) and sig[c + 1][2] == "while":
                    do_tails.add(c + 1)
            continue
        if i in do_tails:
            continue
        o = i + 1
        while (
            o < len(sig)
            and sig[o][1] == "id"
            and sig[o][2] in ("constexpr", "consteval")
        ):
            o += 1
        if o >= len(sig) or sig[o][2] != "(":
            continue
        c = match.get(o)
        if c is None or c + 1 >= len(sig):
            continue
        b = skip_attributes(c + 1)
        if b >= len(sig):
            continue
        after = sig[b][2]
        if after == "{":
            continue
        if tok == "while" and after == ";" and pending_do:
            pending_do -= 1
            continue
        found.append(ln)
    return [(ln, src[ln - 1].strip()) for ln in sorted(set(found))]


# --------------------------------------------------------------------------- command line


def style_settings(style_file):
    """(pinned clang-format version or None, ColumnLimit, IndentWidth) read from the style file."""
    with open(style_file, "r", encoding="utf-8") as f:
        text = f.read()
    first = text.split("\n", 1)[0]
    m = re.search(r"clang-format version (\d+(?:\.\d+)*)", first)
    version = m.group(1) if m else None
    m = re.search(r"^ColumnLimit:\s*(\d+)", text, re.M)
    limit = int(m.group(1)) if m else 140
    m = re.search(r"^IndentWidth:\s*(\d+)", text, re.M)
    width = int(m.group(1)) if m else 2
    return version, limit, width


def binary_version(binary):
    """Version string a clang-format binary reports ('23.1.2'), or None."""
    try:
        p = subprocess.run(
            [binary, "--version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
    except OSError:
        return None
    m = re.search(r"version (\d+(?:\.\d+)*)", p.stdout.decode("utf-8", "replace"))
    return m.group(1) if m else None


def pip_binary():
    """clang-format installed by the pip package 'clang-format' in the running Python environment."""
    try:
        spec = importlib.util.find_spec("clang_format")
    except (ImportError, ValueError):
        spec = None
    if spec is not None and spec.submodule_search_locations:
        for loc in spec.submodule_search_locations:
            path = os.path.join(loc, "data", "bin", "clang-format")
            if os.path.isfile(path):
                return path
    scripts = sysconfig.get_path("scripts")
    if scripts:
        path = os.path.join(scripts, "clang-format")
        if os.path.isfile(path):
            return path
    return None


def find_binary(explicit=None):
    """--clang-format, then $IMTOOL_CLANG_FORMAT, then the pip package, then PATH."""
    if explicit:
        return explicit
    env = os.environ.get("IMTOOL_CLANG_FORMAT")
    if env:
        return env
    return pip_binary() or shutil.which("clang-format")


def run_clang_format(binary, style_file, text):
    # every file is formatted as C++ (.h, .cu and .cuh included)
    p = subprocess.run(
        [binary, "--style=file:" + style_file, "--assume-filename=input.cpp"],
        input=text.encode("utf-8", "surrogateescape"),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if p.returncode != 0:
        raise RuntimeError(
            "clang-format failed: " + p.stderr.decode("utf-8", "replace").strip()
        )
    return p.stdout.decode("utf-8", "surrogateescape")


class NoFixedPoint(RuntimeError):
    pass


def format_text(text, binary, style_file, limit=140, indent_width=2, max_rounds=6):
    """clang-format, then the post-pass, repeated until the result is a fixed point.

    One round is almost always enough.  A second is needed where clang-format lays out a line
    differently once the post-pass has joined its neighbours (trailing comments after a brace).
    clang-format itself does not converge on some inputs (a run of macro invocations without
    semicolons, each followed by a comment, is re-split a little further on every run); for
    those NoFixedPoint is raised and the caller leaves the file untouched, so that every file
    the tool does write is stable under a second run.
    """
    out = post_pass(
        run_clang_format(binary, style_file, text),
        limit=limit,
        indent_width=indent_width,
    )
    for _ in range(max_rounds):
        again = post_pass(
            run_clang_format(binary, style_file, out),
            limit=limit,
            indent_width=indent_width,
        )
        if again == out:
            return out
        out = again
    raise NoFixedPoint(
        f"clang-format does not reach a fixed point after {max_rounds + 1} rounds; file left untouched "
        "(put the region that keeps changing between '// clang-format off' and '// clang-format on')"
    )


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Format C++ files: clang-format with the style file, then the post-pass."
    )
    ap.add_argument(
        "--check",
        action="store_true",
        help="write nothing; print files that would change; exit 1 if any",
    )
    ap.add_argument(
        "--style-file",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "imtool.clang-format"
        ),
    )
    ap.add_argument(
        "--clang-format",
        dest="binary",
        help="clang-format binary (default: $IMTOOL_CLANG_FORMAT, the pip package, PATH)",
    )
    ap.add_argument(
        "--no-brace-check",
        action="store_true",
        help="do not report control statements without braces",
    )
    ap.add_argument("files", nargs="+")
    a = ap.parse_args(argv)

    style_file = os.path.abspath(a.style_file)
    if not os.path.isfile(style_file):
        print(f"imtool_format: style file not found: {style_file}", file=sys.stderr)
        return 2
    pinned, limit, width = style_settings(style_file)
    if pinned is None:
        print(
            f"imtool_format: {style_file}: the first line must name the pinned version ('# clang-format version X.Y.Z')",
            file=sys.stderr,
        )
        return 2
    install = f"pip install clang-format=={pinned}"
    binary = find_binary(a.binary)
    have = binary_version(binary) if binary else None
    if binary is None or have is None:
        print(
            f"imtool_format: no clang-format binary found; install the pinned version with: {install}",
            file=sys.stderr,
        )
        return 2
    if have != pinned:
        print(
            f"imtool_format: {binary} is clang-format {have}; the style file is pinned to {pinned}. Install it with: {install}",
            file=sys.stderr,
        )
        return 2

    would_change = []
    failed = False
    findings = 0
    for path in a.files:
        try:
            with open(
                path, "r", encoding="utf-8", errors="surrogateescape", newline=""
            ) as f:
                src = f.read()
            out = format_text(src, binary, style_file, limit, width)
        except (OSError, RuntimeError) as e:
            print(f"imtool_format: {path}: {e}", file=sys.stderr)
            failed = True
            continue
        if out != src and token_signature(out) != token_signature(src):
            print(
                f"imtool_format: {path}: token sequence would change (clang-format or the post-pass altered a token); file left untouched",
                file=sys.stderr,
            )
            failed = True
            continue
        if not a.no_brace_check:
            # line numbers refer to the file as it is on disk when the command returns
            level = "error" if a.check else "warning"
            for line, stmt in unbraced_bodies(src if a.check else out):
                findings += 1
                print(
                    f"{path}:{line}: {level}: control statement without braces: {stmt}",
                    file=sys.stderr,
                )
        if out == src:
            continue
        would_change.append(path)
        if a.check:
            print(path)
        else:
            tmp = path + ".imtool_tmp"
            with open(
                tmp, "w", encoding="utf-8", errors="surrogateescape", newline=""
            ) as f:
                f.write(out)
            shutil.copymode(path, tmp)
            os.replace(tmp, path)
    if failed:
        return 2
    if findings or (a.check and would_change):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
