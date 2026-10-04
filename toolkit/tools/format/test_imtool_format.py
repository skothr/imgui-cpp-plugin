#!/usr/bin/env python3
"""Unit tests for imtool_format.py.

Run from the repository root:
    python3 -m unittest discover -s toolkit/tools/format -p 'test_*.py'

The post-pass tests feed text shaped like clang-format output (GNU brace layout, 2-space indent)
and need no binary.  Tests that run clang-format use the binary the tool itself would find
($IMTOOL_CLANG_FORMAT, the pip package, PATH) and are skipped when that binary is not the
version pinned in the first line of imtool.clang-format.  With IMTOOL_FORMAT_REQUIRE_BINARY=1
in the environment (set in CI) they fail instead of skipping.
"""

import contextlib
import functools
import io
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import textwrap
import unittest

# The scripts import each other by their path from the repository root, so that the imports
# resolve the same way for Python and for a type checker run from the root.
REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
)
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from toolkit.tools.format import imtool_format as I  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
STYLE = os.path.join(HERE, "imtool.clang-format")
TOOL = os.path.join(HERE, "imtool_format.py")
PINNED, LIMIT, WIDTH = I.style_settings(STYLE)


def pinned_binary():
    binary = I.find_binary()
    if binary and I.binary_version(binary) == PINNED:
        return binary
    return None


BINARY = pinned_binary() or ""
NO_BINARY = f"clang-format {PINNED} not found (pip install clang-format=={PINNED}, or set IMTOOL_CLANG_FORMAT)"
# Set in CI: a missing pinned binary then fails the tests that need it instead of skipping them.
REQUIRE_BINARY = os.environ.get("IMTOOL_FORMAT_REQUIRE_BINARY", "") not in ("", "0")


def needs_binary(obj):
    """Decorator for a test or a test class that runs clang-format.

    Without the pinned binary the test is skipped, or fails when IMTOOL_FORMAT_REQUIRE_BINARY
    is set to anything but '' or '0'.
    """
    if BINARY:
        return obj
    if not REQUIRE_BINARY:
        return unittest.skip(NO_BINARY)(obj)

    def fail(self, *args, **kwargs):
        self.fail(NO_BINARY)

    if isinstance(obj, type):
        obj.setUp = fail
        return obj
    return functools.wraps(obj)(fail)


def d(s):
    return textwrap.dedent(s).lstrip("\n")


def strip_ws(s):
    return re.sub(r"\s+", "", s)


class Base(unittest.TestCase):
    def post(self, src, **kw):
        out = I.post_pass(src, **kw)
        # every post-pass result must keep the tokens
        self.assertEqual(
            strip_ws(src), strip_ws(out), "non-whitespace characters changed"
        )
        self.assertEqual(
            I.token_signature(src), I.token_signature(out), "token sequence changed"
        )
        return out

    def unchanged(self, src, **kw):
        self.assertEqual(src, self.post(src, **kw))


# --------------------------------------------------------------------------- lexer


class LexTests(Base):
    def kinds(self, s):
        return [(k, t) for k, t in I.lex(s) if k not in ("ws", "nl")]

    def test_roundtrip(self):
        src = 'int a = 1; // c\n#define X(a) \\\n  a * a\nauto s = R"x(a "b" )" c)x"; /* m\n n */ char c = \'\\\'\';\n'
        self.assertEqual("".join(t for _, t in I.lex(src)), src)

    def test_string_with_comment_marker(self):
        self.assertEqual(
            self.kinds('s = "// not a comment";'),
            [("id", "s"), ("op", "="), ("str", '"// not a comment"'), ("op", ";")],
        )

    def test_escaped_quote(self):
        self.assertEqual(self.kinds(r'"a\"b" x')[0], ("str", r'"a\"b"'))

    def test_raw_string_spans_lines(self):
        toks = self.kinds('auto s = R"sql(select * from "t"\n where a / b)sql"; int x;')
        self.assertEqual(toks[3][0], "str")
        self.assertTrue(toks[3][1].endswith(')sql"'))
        self.assertEqual(toks[-3:], [("id", "int"), ("id", "x"), ("op", ";")])

    def test_char_literals(self):
        self.assertEqual(
            [t for k, t in self.kinds("'\"' '\\'' '*' u8'a'") if k == "chr"],
            ["'\"'", "'\\''", "'*'", "u8'a'"],
        )

    def test_digit_separator_is_not_a_char_literal(self):
        self.assertEqual(self.kinds("x = 1'000'000 * 2;")[2], ("num", "1'000'000"))

    def test_preprocessor_directive_with_continuation_is_one_token(self):
        toks = self.kinds("#define M(a, b) \\\n  ((a) * (b))\nint x;")
        self.assertEqual(toks[0], ("pp", "#define M(a, b) \\\n  ((a) * (b))"))

    def test_hash_inside_a_line_is_not_a_directive(self):
        self.assertEqual(self.kinds("a # b")[1], ("op", "#"))

    def test_block_comment(self):
        self.assertEqual(
            self.kinds("a /* x * y\n z */ b")[1], ("bc", "/* x * y\n z */")
        )

    def pp(self, s):
        return [t for k, t in I.lex(s) if k == "pp"]

    def test_comment_marker_in_a_directive_string_does_not_start_a_comment(self):
        src = '#define GLOB "src/*"\nint x; /* c */\n'
        self.assertEqual(
            self.kinds(src),
            [
                ("pp", '#define GLOB "src/*"'),
                ("id", "int"),
                ("id", "x"),
                ("op", ";"),
                ("bc", "/* c */"),
            ],
        )
        # without a later '*/' the directive used to run to the end of the file
        self.assertEqual(
            self.pp('#define OPEN "/*"\nif(a) g();\n'), ['#define OPEN "/*"']
        )
        self.assertEqual(
            self.pp('#define S "a\\"/*"\nint x; /* c */\n'), ['#define S "a\\"/*"']
        )
        self.assertEqual(
            self.pp('#include "dir/*.h"\nint x; /* c */\n'), ['#include "dir/*.h"']
        )

    def test_comment_marker_in_a_directive_character_literal(self):
        self.assertEqual(
            self.pp("#define C '/*'\nint x; /* c */\n"), ["#define C '/*'"]
        )
        # the quote in the character literal does not open a string
        self.assertEqual(
            self.pp("#define Q '\"' /* a\n b */ + 1\nint x;\n"),
            ["#define Q '\"' /* a\n b */ + 1"],
        )
        # a digit separator is not a character literal
        self.assertEqual(
            self.pp("#define N 1'000 /* a\n b */\nint x;\n"),
            ["#define N 1'000 /* a\n b */"],
        )

    def test_line_comment_marker_in_a_directive_string(self):
        # the '//' is inside the string, so the block comment after it belongs to the directive
        self.assertEqual(
            self.pp('#define URL "http://x" /* a\n b */\nint x;\n'),
            ['#define URL "http://x" /* a\n b */'],
        )
        self.assertEqual(
            self.pp("#define SL '//' /* a\n b */\nint x;\n"),
            ["#define SL '//' /* a\n b */"],
        )

    def test_comment_marker_in_a_continued_directive_string(self):
        src = '#define M(x) \\\n  f("a/*", \\\n    x)\nint y; /* c */\n'
        self.assertEqual(self.pp(src), ['#define M(x) \\\n  f("a/*", \\\n    x)'])
        src = '#define M(x) \\\r\n  f("//", x) /* a\r\n b */\r\nint y;\r\n'
        self.assertEqual(
            self.pp(src), ['#define M(x) \\\r\n  f("//", x) /* a\r\n b */']
        )

    def test_comment_marker_in_a_directive_raw_string(self):
        self.assertEqual(
            self.pp('#define R R"x(/* " )x"\nint y; /* c */\n'),
            ['#define R R"x(/* " )x"'],
        )

    def test_real_comments_in_a_directive_still_end_or_extend_it(self):
        self.assertEqual(
            self.pp("#define A 1 /* a\n b */ + 2\nint x;\n"),
            ["#define A 1 /* a\n b */ + 2"],
        )
        self.assertEqual(
            self.pp("#define A 1 // c /*\nint x; /* d */\n"), ["#define A 1 // c /*"]
        )
        self.assertEqual(
            self.pp("#error don't /* a */ do this\nint x;\n"),
            ["#error don't /* a */ do this"],
        )

    def test_swallowed_code_was_hidden_from_the_brace_check(self):
        src = '#define OPEN "/*"\nvoid f()\n{\n  if(a) g();\n}\n'
        self.assertEqual([ln for ln, _ in I.unbraced_bodies(src)], [4])

    def test_token_signature_detects_merged_words_and_new_comment(self):
        self.assertNotEqual(I.token_signature("int x;"), I.token_signature("intx;"))
        self.assertNotEqual(I.token_signature("a / *p;"), I.token_signature("a /*p;"))
        self.assertEqual(
            I.token_signature("vector<vector<int> > v;"),
            I.token_signature("vector<vector<int>> v;"),
        )
        self.assertEqual(I.token_signature("a  *  b ;\n"), I.token_signature("a*b;"))


# --------------------------------------------------------------------------- braces

LAMBDA_HEAD = "void f()\n{\n  run([&](int a)\n    {\n"
LAMBDA_TAIL = "    });\n}\n"


class BraceTests(Base):
    def test_try_catch_braces_stay_indented(self):
        self.unchanged(
            d("""
            void f()
            {
              try
                {
                  g();
                  h();
                }
              catch(const std::exception &e)
                {
                  report(e);
                  throw;
                }
            }
            """)
        )

    def test_enum_braces_are_indented(self):
        src = d("""
            enum class Mode
            {
              A = 0,
              B,
            };
            int x;
            """)
        want = d("""
            enum class Mode
              {
                A = 0,
                B,
              };
            int x;
            """)
        self.assertEqual(self.post(src), want)
        self.assertEqual(self.post(want), want)

    def test_struct_class_function_namespace_braces_are_not_touched(self):
        self.unchanged(
            d("""
            namespace n
            {
              struct S
              {
                int a;
                int b;
              };
              class C
              {
              public:
                void f();
              };
              void g()
              {
                h();
                k();
                m();
                n();
              }
            }
            """)
        )

    def test_lambda_brace_moves_one_level_out(self):
        src = d("""
            void f()
            {
              auto g = [&](int a)
                {
                  foo();
                  return a + 1;
                };
              std::sort(v.begin(), v.end(),
                        [](const A &a, const A &b)
                          {
                            foo();
                            return a.orb < b.orb;
                          });
            }
            """)
        want = d("""
            void f()
            {
              auto g = [&](int a)
              {
                foo();
                return a + 1;
              };
              std::sort(v.begin(), v.end(),
                        [](const A &a, const A &b)
                        {
                          foo();
                          return a.orb < b.orb;
                        });
            }
            """)
        self.assertEqual(self.post(src), want)

    def test_subscript_before_brace_is_not_a_lambda(self):
        self.unchanged("void f()\n{\n  if(a[i])\n    {\n      g(); // why\n    }\n}\n")

    def test_attribute_before_a_brace_is_not_a_lambda(self):
        # the layouts clang-format gives a block that follows a statement attribute
        self.unchanged(
            d("""
            void f(int x)
            {
              switch(x)
                {
                case 1:
                  [[likely]]
                  {
                    a();
                    b();
                    c();
                    d();
                  }
                default: e();
                }
              g();
              [[likely]] /* why */
              {
                a();
                b();
                c();
                d();
              }
              h();
              [[gnu::hot]] // why
              {
                a();
                b();
                c();
                d();
              }
              if(x) [[likely]]
                {
                  g(); // why
                }
              else [[unlikely]]
                {
                  h(); // why
                }
            }
            """)
        )

    def test_lambda_with_an_attribute_is_still_a_lambda(self):
        src = "void f()\n{\n  auto m = [](int q) [[gnu::cold]]\n    {\n      a();\n      return q;\n    };\n}\n"
        want = "void f()\n{\n  auto m = [](int q) [[gnu::cold]]\n  {\n    a();\n    return q;\n  };\n}\n"
        self.assertEqual(self.post(src), want)

    def lambda_moves_out(self, head):
        src = f"void f()\n{{\n  {head}\n    {{\n      foo();\n      return x + 1;\n    }};\n}}\n"
        want = (
            f"void f()\n{{\n  {head}\n  {{\n    foo();\n    return x + 1;\n  }};\n}}\n"
        )
        self.assertEqual(self.post(src), want, head)

    def test_lambda_with_specifiers_and_a_return_type(self):
        self.lambda_moves_out("auto a = [&](int x) -> int")
        self.lambda_moves_out("auto a = [&](int x) -> std::vector<std::pair<int, T*>>")
        self.lambda_moves_out("auto a = [&](int x) -> decltype(x + 1)")
        self.lambda_moves_out("auto a = [&](int x) noexcept(true)")
        self.lambda_moves_out("auto a = [&](int x) noexcept(noexcept(g(x))) -> int")
        self.lambda_moves_out("auto a = [&](int x) static")
        self.lambda_moves_out("auto a = [&](int x) consteval")
        self.lambda_moves_out(
            "auto a = [&](int x) mutable noexcept -> std::vector<int>"
        )
        self.lambda_moves_out("auto a = [&]<typename T>(T x) requires C<T>")
        self.lambda_moves_out(
            "auto a = [&]<typename T>(T x)\n    requires C<T> && D<T>"
        )
        self.lambda_moves_out("auto a = [] [[nodiscard]] (int x)")
        self.lambda_moves_out("auto a = [x] mutable")
        self.lambda_moves_out("return [&](int x) -> int")

    def test_function_with_a_subscript_and_a_return_type_is_not_a_lambda(self):
        self.unchanged(
            "struct S\n{\n  auto operator[](int i) -> T&\n  {\n    check(i); // why\n    return m_d[i];\n  }\n"
            "  auto f(int a[3]) -> int\n  {\n    check(a); // why\n    return a[0];\n  }\n};\n"
        )

    def test_do_while_tail_is_joined(self):
        src = "void f()\n{\n  do\n    {\n      x++;\n      y--;\n    }\n  while(x < 3);\n}\n"
        out = self.post(src)
        self.assertIn("    } while(x < 3);\n", out)
        self.assertNotIn("\n  while(x < 3);", out)

    def test_plain_while_after_a_block_is_not_joined(self):
        self.unchanged(
            "void f()\n{\n  if(a)\n    {\n      g(); // why\n    }\n  while(x < 3) { x--; }\n}\n"
        )

    def test_block_comment_inside_shifted_block_moves_with_it(self):
        src = (
            LAMBDA_HEAD
            + "      /* first\n         second */\n      g();\n"
            + LAMBDA_TAIL
        )
        # only the line's indentation moves; the later lines of the comment are inside the
        # token and stay as written, as they do under clang-format
        self.assertIn(
            "  {\n    /* first\n         second */\n    g();\n  });\n", self.post(src)
        )

    def test_block_comment_at_column_zero_does_not_block_the_shift(self):
        src = LAMBDA_HEAD + "      /* first\nsecond */\n      g();\n" + LAMBDA_TAIL
        self.assertIn("  {\n    /* first\nsecond */\n    g();\n  });\n", self.post(src))

    def test_block_comment_after_a_trailing_comment_keeps_its_column(self):
        src = (
            LAMBDA_HEAD
            + "      g(); // one\n      /* two */\n      h(); /* three */\n      /* four */\n      k();\n"
            + LAMBDA_TAIL
        )
        self.assertIn(
            "    g(); // one\n      /* two */\n    h(); /* three */\n      /* four */\n    k();\n",
            self.post(src),
        )

    def test_block_comment_line_in_enum_keeps_its_column(self):
        src = "enum E\n{\n  A, // first\n  /* block */\n  B,\n  /* multi\n     line */\n  C,\n};\n"
        want = "enum E\n  {\n    A, // first\n  /* block */\n    B,\n  /* multi\n     line */\n    C,\n  };\n"
        self.assertEqual(self.post(src), want)
        self.assertEqual(self.post(want), want)


class ShiftStabilityTests(Base):
    """Lines clang-format does not re-indent must not be moved, or every run would move them again."""

    def test_comment_line_in_enum_keeps_its_column(self):
        src = "enum E\n{\n  A, // first\n\n          // group\n  B, // second\n  // plain\n  C,\n};\n"
        want = "enum E\n  {\n    A, // first\n\n          // group\n    B, // second\n  // plain\n    C,\n  };\n"
        self.assertEqual(self.post(src), want)

    def test_enum_is_not_shifted_when_a_line_would_pass_the_limit(self):
        def enum(name):
            return f"enum E\n{{\n  {name} = 0,\n  B,\n}};\n"

        fits = "A" * (LIMIT - WIDTH - len("   = 0,"))
        self.assertEqual(len(f"  {fits} = 0,") + WIDTH, LIMIT)
        out = self.post(enum(fits), limit=LIMIT, indent_width=WIDTH)
        self.assertEqual(out, f"enum E\n  {{\n    {fits} = 0,\n    B,\n  }};\n")
        self.assertEqual(max(len(x) for x in out.split("\n")), LIMIT)
        # one column more: the shift would put the line past the limit, so the enum stays
        self.unchanged(enum(fits + "A"), limit=LIMIT, indent_width=WIDTH)
        # a line that is already past the limit does not block the shift
        long = "A" * LIMIT
        self.assertEqual(
            self.post(enum(long), limit=LIMIT, indent_width=WIDTH),
            f"enum E\n  {{\n    {long} = 0,\n    B,\n  }};\n",
        )

    def test_continuation_of_a_trailing_comment_keeps_its_column(self):
        src = (
            LAMBDA_HEAD
            + "      g(); // one\n           // two\n      h();\n"
            + LAMBDA_TAIL
        )
        self.assertIn("    g(); // one\n           // two\n    h();\n", self.post(src))

    def test_ordinary_comment_moves_with_the_code(self):
        src = LAMBDA_HEAD + "      // note\n      g();\n" + LAMBDA_TAIL
        self.assertIn("  {\n    // note\n    g();\n  });\n", self.post(src))


class FixedPointTests(unittest.TestCase):
    def with_fake_clang_format(self, fake, fn):
        orig = I.run_clang_format
        I.run_clang_format = fake
        try:
            return fn()
        finally:
            I.run_clang_format = orig

    def test_no_fixed_point_is_reported_not_written(self):
        calls = []

        def growing(binary, style, text):
            calls.append(1)
            return text + "\n"

        with self.assertRaises(I.NoFixedPoint):
            self.with_fake_clang_format(
                growing, lambda: I.format_text("int x;\n", "unused", "unused")
            )
        self.assertGreater(len(calls), 2)

    def test_stable_formatter_needs_two_calls(self):
        calls = []

        def stable(binary, style, text):
            calls.append(1)
            return text

        out = self.with_fake_clang_format(
            stable, lambda: I.format_text("int x;\n", "unused", "unused")
        )
        self.assertEqual(out, "int x;\n")
        self.assertEqual(len(calls), 2)


# --------------------------------------------------------------------------- bodies


def control(head, body, indent="  "):
    """clang-format's expanded form of a control statement inside a function."""
    lines = (
        [indent + head, indent + "  {"]
        + [indent + "    " + b for b in body]
        + [indent + "  }"]
    )
    return "void f()\n{\n" + "\n".join(lines) + "\n}\n"


class BodyTests(Base):
    LONG = "if(veryLongConditionNumberOne && veryLongConditionNumberTwo && veryLongConditionNumberThree && veryLongConditionNumberFour)"

    def test_one_statement_that_fits_goes_on_the_statement_line(self):
        self.assertEqual(
            self.post(control("if(a)", ["g();"])), "void f()\n{\n  if(a) { g(); }\n}\n"
        )

    def test_two_and_three_statements_that_fit_go_on_the_statement_line(self):
        self.assertEqual(
            self.post(
                control("for(auto n : mNodes)", ["n->disconnectAll();", "delete n;"])
            ),
            "void f()\n{\n  for(auto n : mNodes) { n->disconnectAll(); delete n; }\n}\n",
        )
        self.assertEqual(
            self.post(control("if(a)", ["g();", "h();", "return k;"])),
            "void f()\n{\n  if(a) { g(); h(); return k; }\n}\n",
        )

    def test_four_statements_stay_expanded(self):
        self.unchanged(control("if(a)", ["g();", "h();", "k();", "m();"]))

    def test_else_else_if_and_while(self):
        src = d("""
            void f()
            {
              if(a) { g(); }
              else if(b)
                {
                  h();
                  k();
                }
              else
                {
                  m();
                }
              while(x)
                {
                  x--;
                }
            }
            """)
        want = d("""
            void f()
            {
              if(a) { g(); }
              else if(b) { h(); k(); }
              else { m(); }
              while(x) { x--; }
            }
            """)
        self.assertEqual(self.post(src), want)

    def test_line_of_exactly_the_limit_is_joined_and_one_more_is_not(self):
        # the limit the tool passes: ColumnLimit of the style file
        base = len("  if(a) { g(); ") + len(" }")
        fits = "x" * (LIMIT - base - 3) + "();"
        joined = "  if(a) { g(); " + fits + " }"
        self.assertEqual(len(joined), LIMIT)
        self.assertEqual(
            self.post(control("if(a)", ["g();", fits]), limit=LIMIT),
            "void f()\n{\n" + joined + "\n}\n",
        )
        over = "x" + fits
        out = self.post(control("if(a)", ["g();", over]), limit=LIMIT)
        self.assertEqual(out, "void f()\n{\n  if(a)\n    { g(); " + over + " }\n}\n")

    def test_limit_argument_moves_the_boundary(self):
        src = control("if(a)", ["g();"])
        joined = "  if(a) { g(); }"
        self.assertIn(joined + "\n", self.post(src, limit=len(joined)))
        self.assertEqual(
            self.post(src, limit=len(joined) - 1),
            "void f()\n{\n  if(a)\n    { g(); }\n}\n",
        )

    def test_body_alone_on_the_next_line_when_the_statement_line_is_too_long(self):
        one = control(
            self.LONG, ["doSomethingQuiteLongHere(argumentOne, argumentTwo);"]
        )
        want = (
            "void f()\n{\n  "
            + self.LONG
            + "\n    { doSomethingQuiteLongHere(argumentOne, argumentTwo); }\n}\n"
        )
        self.assertEqual(self.post(one), want)
        self.assertEqual(self.post(want), want)
        three = control(self.LONG, ["a = 1;", "b = 2;", "return c;"])
        self.assertEqual(
            self.post(three),
            "void f()\n{\n  " + self.LONG + "\n    { a = 1; b = 2; return c; }\n}\n",
        )

    def test_expanded_when_the_body_line_is_too_long_as_well(self):
        body = (
            "doSomething("
            + ", ".join(["argumentNumber%d" % i for i in range(9)])
            + ");"
        )
        self.assertGreater(len("    { " + body + " }"), LIMIT)
        self.unchanged(control(self.LONG, [body]), limit=LIMIT)
        half = (
            "doSomething("
            + ", ".join(["argumentNumber%d" % i for i in range(4)])
            + ");"
        )
        self.assertGreater(len("    { " + half + " " + half + " }"), LIMIT)
        self.unchanged(control("if(a)", [half, half]), limit=LIMIT)

    def test_statement_spanning_lines_gets_the_body_on_the_next_line(self):
        src = "void f()\n{\n  if(aaaaaaaaaa &&\n     bbbbbbbbbb)\n    {\n      g();\n      h();\n    }\n}\n"
        self.assertEqual(
            self.post(src),
            "void f()\n{\n  if(aaaaaaaaaa &&\n     bbbbbbbbbb)\n    { g(); h(); }\n}\n",
        )

    def test_comment_in_block_blocks_the_join(self):
        self.unchanged(control("if(a)", ["g(); // why"]))
        self.unchanged(control("if(a)", ["// why", "g();"]))
        self.unchanged("void f()\n{\n  if(a)\n    { // why\n      g();\n    }\n}\n")
        self.unchanged(control("if(a)", ["/* why */", "g();"]))

    def test_preprocessor_line_in_block_blocks_the_join(self):
        self.unchanged(
            "void f()\n{\n  if(a)\n    {\n#ifdef X\n      g();\n#endif\n    }\n}\n"
        )
        self.unchanged("void g()\n{\n#ifdef X\n  h();\n#endif\n}\n")

    def test_nested_block_blocks_the_join(self):
        self.unchanged(control("if(a)", ["for(auto n : nodes) { n->step(); }"]))
        self.unchanged(control("if(a)", ["x = 1;", "if(b) { y = 2; }"]))
        self.unchanged(control("if(a)", ["v = { 1, 2 };"]))

    def test_braceless_statement_in_block_blocks_the_join(self):
        self.unchanged(control("if(a)", ["if(b) g();"]))

    def test_comment_on_the_statement_line_puts_the_body_on_the_next_line(self):
        src = "void f()\n{\n  if(a) // why\n    {\n      g();\n    }\n}\n"
        self.assertEqual(
            self.post(src), "void f()\n{\n  if(a) // why\n    { g(); }\n}\n"
        )

    def test_switch_try_do_and_bare_blocks_are_left_alone(self):
        self.unchanged(
            "void f()\n{\n  switch(x)\n    {\n    default: break;\n    }\n  {\n    g();\n  }\n  try\n    {\n      g();\n    }\n  catch(...)\n    {\n      h();\n    }\n}\n"
        )

    def test_function_bodies_up_to_three_statements(self):
        self.assertEqual(
            self.post("void f()\n{\n  g();\n  h();\n}\n"), "void f() { g(); h(); }\n"
        )
        self.assertEqual(
            self.post(
                "  Rect& operator=(const Rect &o)\n  {\n    p1 = o.p1;\n    p2 = o.p2;\n    return *this;\n  }\n"
            ),
            "  Rect& operator=(const Rect &o) { p1 = o.p1; p2 = o.p2; return *this; }\n",
        )
        self.unchanged("void f()\n{\n  g();\n  h();\n  k();\n  m();\n}\n")

    def test_function_body_alone_on_the_next_line(self):
        sig = "void veryLongFunctionNameNumberOne(int argumentNumberOne, int argumentNumberTwo, int argumentNumberThree, int argumentNumberFour)"
        src = (
            sig
            + "\n{\n  doSomethingQuiteLongHere(argumentOne, argumentTwo);\n  return;\n}\n"
        )
        self.assertEqual(
            self.post(src),
            sig + "\n{ doSomethingQuiteLongHere(argumentOne, argumentTwo); return; }\n",
        )

    def test_constructor_with_initializers_on_their_own_line(self):
        src = "Foo::Foo(int a, int b)\n  : m_a(a), m_b(b)\n{\n  init();\n}\n"
        self.assertEqual(
            self.post(src), "Foo::Foo(int a, int b)\n  : m_a(a), m_b(b)\n{ init(); }\n"
        )
        self.unchanged("Foo::Foo(int a, int b)\n  : m_a(a), m_b(b)\n{ }\n")

    def test_body_after_a_template_line_joins_the_declaration(self):
        src = "  template<typename U>\n  Rect(const Rect<U> &o) : p1(o.p1), p2(o.p2)\n  { }\n"
        self.assertEqual(
            self.post(src),
            "  template<typename U>\n  Rect(const Rect<U> &o) : p1(o.p1), p2(o.p2) { }\n",
        )
        src = "template<typename U>\nvoid g(U u)\n{ x = u; }\n"
        self.assertEqual(
            self.post(src), "template<typename U>\nvoid g(U u) { x = u; }\n"
        )
        src = "  template<typename U>\n  Rect& operator=(const Rect<U> &o)\n  {\n    p1 = o.p1;\n    return *this;\n  }\n"
        self.assertEqual(
            self.post(src),
            "  template<typename U>\n  Rect& operator=(const Rect<U> &o) { p1 = o.p1; return *this; }\n",
        )

    def test_member_function_after_an_access_specifier(self):
        src = "class C\n{\npublic:\n  int get() const\n  {\n    check();\n    return m_a;\n  }\n};\n"
        self.assertEqual(
            self.post(src),
            "class C\n{\npublic:\n  int get() const { check(); return m_a; }\n};\n",
        )

    def joins(self, head, indent=""):
        src = f"{indent}{head}\n{indent}{{\n{indent}  check();\n{indent}  return n;\n{indent}}}\n"
        self.assertEqual(
            self.post(src), f"{indent}{head} {{ check(); return n; }}\n", head
        )

    def test_function_declared_with_template_class_or_an_elaborated_type(self):
        src = "template<class T>\nT get()\n{\n  check();\n  return T();\n}\n"
        self.assertEqual(
            self.post(src), "template<class T>\nT get() { check(); return T(); }\n"
        )
        self.joins("template<class T, typename U>\nT get()")
        self.joins("struct Foo* make()")
        self.joins("static enum E pick(int a)")
        self.joins("std::vector<struct Foo*> all()")

    def test_operator_functions_with_long_names(self):
        self.joins("void* operator new(std::size_t n)")
        self.joins("void* operator new[](std::size_t n)")
        self.joins("void operator delete[](void *p)")
        self.joins("operator const T*() const", "  ")
        self.joins("operator unsigned long long() const", "  ")
        self.joins("operator std::vector<int>() const", "  ")
        self.joins("bool operator<(const A &o) const", "  ")
        self.joins("bool operator>>(const A &o) const", "  ")
        self.joins("int operator()(int a) const", "  ")

    def test_function_with_a_trailing_return_type_or_other_tail(self):
        self.joins("auto size() const -> std::size_t", "  ")
        self.joins("auto get() -> std::map<int, std::vector<T>>&", "  ")
        self.joins("auto next() noexcept -> decltype(m_it + 1)", "  ")
        self.joins("int& get() &", "  ")
        self.joins("int&& take() &&", "  ")
        self.joins("int get() const noexcept(true)", "  ")
        self.joins("void h() [[gnu::cold]]")
        self.joins("template<typename T>\nrequires C<T> T get()")
        src = "  int f()\n    requires C<T>\n  {\n    check();\n    return n;\n  }\n"
        self.assertEqual(
            self.post(src), "  int f()\n    requires C<T>\n  { check(); return n; }\n"
        )

    def test_if_constexpr_body_is_joined(self):
        self.assertEqual(
            self.post(control("if constexpr(sizeof(T) == 4)", ["g();", "h();"])),
            "void f()\n{\n  if constexpr(sizeof(T) == 4) { g(); h(); }\n}\n",
        )
        src = "void f()\n{\n  if constexpr(a) { g(); }\n  else if constexpr(b)\n    {\n      h();\n      k();\n    }\n}\n"
        self.assertEqual(
            self.post(src),
            "void f()\n{\n  if constexpr(a) { g(); }\n  else if constexpr(b) { h(); k(); }\n}\n",
        )

    def test_enum_and_record_heads_ending_in_a_parenthesis_are_not_functions(self):
        self.unchanged("  enum class E : decltype(x)\n    {\n      A,\n    };\n")
        self.unchanged("struct S : Base<decltype(x)>, Other(y)\n{\n  int a;\n};\n")

    def test_records_namespaces_and_initializers_are_not_functions(self):
        self.unchanged("struct S\n{\n  int a;\n};\nnamespace n\n{\n  int a;\n}\n")
        self.unchanged("static const int table[] =\n{\n  1,\n};\n")
        self.unchanged("auto x = make(a)\n{\n  1,\n};\n")


# --------------------------------------------------------------------------- pointers, operators


class SpacingTests(Base):
    def line(self, s, **kw):
        return self.post(s + "\n", **kw).rstrip("\n")

    def test_pointer_in_template_arguments(self):
        self.assertEqual(self.line("std::vector<Node *> v;"), "std::vector<Node*> v;")
        self.assertEqual(
            self.line("std::map<int, Node *> m;"), "std::map<int, Node*> m;"
        )
        self.assertEqual(
            self.line("std::function<void(const T &)> f;"),
            "std::function<void(const T&)> f;",
        )
        self.assertEqual(self.line("std::vector<char **> v;"), "std::vector<char**> v;")

    def test_pointer_in_cast_and_unnamed_parameter(self):
        self.assertEqual(self.line("int *q = (int *)ptr;"), "int *q = (int*)ptr;")
        self.assertEqual(
            self.line("void f(int *, const T &);"), "void f(int*, const T&);"
        )

    def test_named_declarators_keep_binding_right(self):
        self.assertEqual(
            self.line("void f(const std::vector<Node *> &nn, Node *n);"),
            "void f(const std::vector<Node*> &nn, Node *n);",
        )
        self.unchanged("Node *n = nullptr;\n")
        self.unchanged("  const T &x = y;\n")

    def test_return_type_binds_left(self):
        self.assertEqual(
            self.line("Node *NodeGraph::find(int id)"), "Node* NodeGraph::find(int id)"
        )
        self.assertEqual(
            self.line(
                "const std::vector<Node *> &NodeGraph::nodes() const { return mNodes; }"
            ),
            "const std::vector<Node*>& NodeGraph::nodes() const { return mNodes; }",
        )
        self.assertEqual(
            self.line("  virtual Node *clone() const = 0;"),
            "  virtual Node* clone() const = 0;",
        )
        self.assertEqual(
            self.line("  T &operator[](int i) { return data[i]; }"),
            "  T& operator[](int i) { return data[i]; }",
        )
        self.assertEqual(
            self.line("friend std::ostream &operator<<(std::ostream &os, const V &v);"),
            "friend std::ostream& operator<<(std::ostream &os, const V &v);",
        )
        self.assertEqual(
            self.line("template<typename T> T *make(int n);"),
            "template<typename T> T* make(int n);",
        )
        self.assertEqual(
            self.line("  [[nodiscard]] NodeGraph *graph() const { return m_graph; }"),
            "  [[nodiscard]] NodeGraph* graph() const { return m_graph; }",
        )
        self.assertEqual(
            self.line("[[nodiscard]] static const Vec2f &center();"),
            "[[nodiscard]] static const Vec2f& center();",
        )

    def test_pointer_after_nested_template_arguments(self):
        self.assertEqual(
            self.line("std::map<K, std::vector<V>> &get();"),
            "std::map<K, std::vector<V>>& get();",
        )
        self.assertEqual(
            self.line("auto p = (std::vector<std::vector<int>> *)q;"),
            "auto p = (std::vector<std::vector<int>>*)q;",
        )
        self.assertEqual(
            self.line("void h(std::vector<std::vector<int>> &, int);"),
            "void h(std::vector<std::vector<int>>&, int);",
        )
        self.unchanged("  x = a >> *p;\n  y = (b >> *q);\n")

    def test_direct_initialised_variable_keeps_binding_right(self):
        for decl in (
            "Foo &ref(other);",
            "T *p(nullptr);",
            "auto &x(y);",
            "T *p(a, b);",
            "Node *n(graph.find(id));",
            "const T &r(*it);",
            "T *p(0), *q(0);",
            "Foo *f(new Foo);",
            "std::string &s(m_names[key]);",
            "T *p(static_cast<T *>(q));",
            "float *v(a * b);",
            "bool &b(a > c);",
            "bool &b(a >> c);",
        ):
            want = decl.replace("<T *>", "<T*>").replace("a * b", "a*b")
            self.assertEqual(self.line("  " + decl), "  " + want)

    def test_function_declaration_is_told_from_a_variable_by_its_parameters(self):
        for decl, want in (
            ("T *make(int n);", "T* make(int n);"),
            ("Node *find();", "Node* find();"),
            ("Node *find(int);", "Node* find(int);"),
            ("Node *find(const Key &);", "Node* find(const Key&);"),
            ("Node *find(Key *, int);", "Node* find(Key*, int);"),
            ("Node *find(Key k, Graph &g);", "Node* find(Key k, Graph &g);"),
            ("Node *find(std::vector<int> &v);", "Node* find(std::vector<int> &v);"),
            ("Node *find(Args...);", "Node* find(Args...);"),
            (
                "Node *add(std::unique_ptr<Node> node);",
                "Node* add(std::unique_ptr<Node> node);",
            ),
            (
                "Node *add(std::map<int, std::vector<T>> m);",
                "Node* add(std::map<int, std::vector<T>> m);",
            ),
            ("Node *find(id) const;", "Node* find(id) const;"),
            ("Node *find(id) { return m[id]; }", "Node* find(id) { return m[id]; }"),
            ("Node *Graph::find(id);", "Node* Graph::find(id);"),
            ("Node *find(id)", "Node* find(id)"),
            ("Node *find(int id,", "Node* find(int id,"),
        ):
            self.assertEqual(self.line(decl), want)

    def test_return_type_padding_is_kept_as_width(self):
        self.assertEqual(
            self.line("  Node   *find(int id);"), "  Node*   find(int id);"
        )

    def test_not_a_return_type(self):
        self.unchanged("  return *find(id);\n")
        self.unchanged("  Node *n = find(id);\n")
        self.unchanged("  x = y * find(id);\n", tight_ops=False)
        self.unchanged("  delete *it(a);\n")
        self.unchanged("  else *p(a) = 3;\n")

    def test_binary_multiplicative_operators_are_tight(self):
        self.assertEqual(
            self.line("  float a = b * c / d % e + f * (g - h);"),
            "  float a = b*c/d%e + f*(g - h);",
        )
        self.assertEqual(self.line("  x = (a + b) * 2.0f;"), "  x = (a + b)*2.0f;")
        self.assertEqual(self.line("  x = a * -b;"), "  x = a*-b;")

    def test_additive_and_compound_operators_keep_spaces(self):
        self.unchanged("  x = a + b - c;\n  x *= 2;\n  y /= z;\n")

    def test_joins_that_would_make_another_token_are_refused(self):
        self.unchanged("  y = a / *p;\n")  # would start a comment
        self.unchanged("  y = a * *p;\n")
        self.unchanged("  y = a % ::b;\n")  # would start the digraph %:
        self.unchanged("  y = a * &b;\n")

    def test_declarators_are_not_operators(self):
        self.unchanged("  Node *n;\n  T &&r = f();\n  void g(int *p, int &q);\n")

    def test_tight_ops_can_be_switched_off(self):
        self.unchanged("  x = a * b;\n", tight_ops=False)


# --------------------------------------------------------------------------- must not misfire


class ImmunityTests(Base):
    def test_string_literals(self):
        self.unchanged('  const char *s = "a * b / c std::vector<Node *> (int *)p";\n')
        self.unchanged(
            '  log("enum X\\n{\\n};\\n  if(x)\\n    {\\n      y;\\n    }");\n'
        )

    def test_raw_string_spanning_lines(self):
        raw = 'R"(\n  enum X\n  {\n    a * b, std::vector<T *>\n  };\n  if(a)\n    {\n      g();\n    }\n)"'
        src = LAMBDA_HEAD + "      auto s = " + raw + ";\n      g();\n" + LAMBDA_TAIL
        out = self.post(src)
        self.assertIn(raw, out)  # the literal is byte-identical
        self.assertIn("  {\n    auto s = R", out)  # the code around it still moved

    def test_character_literals(self):
        self.unchanged(
            "  char c = '*'; char q = '\"'; char s = '/'; if(c == '{') { n++; }\n"
        )

    def test_line_comments(self):
        self.unchanged("  // x = a * b; std::vector<Node *> v; Node *find(int id)\n")
        self.unchanged("  x = 1; // a * b and (int *)p\n")
        self.unchanged(
            "// if(a)\n//   {\n//     g();\n//   }\n// enum E\n// {\n// };\n"
        )

    def test_block_comments(self):
        self.unchanged(
            "/* enum X\n{\n  A * B\n};\nif(a)\n  {\n    g();\n  }\n*/\nint x;\n"
        )
        self.unchanged("  int y; /* a * b */\n")

    def test_preprocessor_lines(self):
        self.unchanged("#define MUL(a, b) ((a) * (b))\n")
        self.unchanged("#define PTR(T) std::vector<T *>\n")
        self.unchanged(
            "#define BLOCK(x) \\\n  if(x)        \\\n    {          \\\n      g();     \\\n    }\n"
        )
        self.unchanged(
            "#define E(n) \\\n  enum n     \\\n  {          \\\n    A,       \\\n  };\n"
        )
        self.unchanged("#if defined(A) && B * 2 > 4\n#endif\n")
        self.unchanged("#include <a * b>\n")

    def test_preprocessor_inside_a_shifted_block_is_not_indented(self):
        src = LAMBDA_HEAD + "#ifdef A\n      g();\n#endif\n" + LAMBDA_TAIL
        self.assertIn("  {\n#ifdef A\n    g();\n#endif\n  });\n", self.post(src))

    def test_clang_format_off_region_is_untouched(self):
        region = d("""
            // clang-format off
            enum Keep
            {
              A  =  a * b,
            };
            std::vector<Node *> keep;
            Node *keepFind(int id);
            void k()
            {
              if(a)
                {
                  g();
                }
              h();
            }
            // clang-format on
            """)
        src = region + "std::vector<Node *> change;\n"
        out = self.post(src)
        self.assertTrue(out.startswith(region))
        self.assertTrue(out.endswith("std::vector<Node*> change;\n"))

    def test_block_comment_form_of_the_switch(self):
        region = "/* clang-format off */\nint a = b * c;\n/* clang-format on */\n"
        self.assertEqual(
            self.post(region + "int d = e * f;\n"), region + "int d = e*f;\n"
        )

    def test_protected_lines_inside_a_shifted_block_stay(self):
        src = (
            LAMBDA_HEAD
            + "      // clang-format off\n      x  =  a * b;\n      // clang-format on\n      g();\n"
            + LAMBDA_TAIL
        )
        out = self.post(src)
        self.assertIn(
            "      // clang-format off\n      x  =  a * b;\n      // clang-format on\n    g();\n",
            out,
        )

    def test_byte_order_mark_before_the_first_directive(self):
        bom = "﻿"
        self.assertEqual(
            [(k, t) for k, t in I.lex(bom + "#define AREA w * h\n")][:2],
            [("bom", bom), ("pp", "#define AREA w * h")],
        )
        self.unchanged(bom + "#define AREA w * h\nint x = a*b;\n")
        self.assertEqual(self.post(bom + "int x = a * b;\n"), bom + "int x = a*b;\n")
        self.unchanged(bom + "enum E\n  {\n    A,\n  };\n")
        self.assertEqual(
            self.post(bom + "enum E\n{\n  A,\n};\n"),
            bom + "enum E\n  {\n    A,\n  };\n",
        )
        # a mark that is removed or added counts as a token change
        self.assertNotEqual(
            I.token_signature(bom + "int x;"), I.token_signature("int x;")
        )
        self.assertEqual(I.unbraced_bodies(bom + "#define M(x) if(x) g()\n"), [])

    def test_crlf_line_endings_are_kept(self):
        src = "void f()\r\n{\r\n  if(a)\r\n    {\r\n      g();\r\n    }\r\n  h();\r\n  k();\r\n  m();\r\n  n();\r\n}\r\n"
        self.assertEqual(
            self.post(src),
            "void f()\r\n{\r\n  if(a) { g(); }\r\n  h();\r\n  k();\r\n  m();\r\n  n();\r\n}\r\n",
        )

    def test_unbalanced_and_empty_input(self):
        self.unchanged("")
        self.unchanged("}\n")
        self.unchanged("void f()\n{\n")
        self.unchanged("int x;")  # no final newline


# --------------------------------------------------------------------------- brace check


class BraceCheckTests(unittest.TestCase):
    def lines(self, src):
        return [ln for ln, _ in I.unbraced_bodies(src)]

    def test_braced_statements_are_clean(self):
        src = d("""
            void f()
            {
              if(a) { g(); }
              else if(b) { h(); }
              else { k(); }
              for(int i = 0; i < n; i++) { g(); }
              while(x) { x--; }
              do { x++; } while(x < 3);
              do
                {
                  x++;
                } while(x < 3);
              if constexpr(c) { g(); }
            }
            """)
        self.assertEqual(I.unbraced_bodies(src), [])

    def test_each_unbraced_form_is_found_with_its_line(self):
        src = d("""
            void f()
            {
              if(a) g();
              else if(b) h();
              else k();
              for(int i = 0; i < n; i++) g();
              while(x) x--;
              do x++; while(x < 3);
              if(a)
                g();
              while(spin());
            }
            """)
        found = I.unbraced_bodies(src)
        self.assertEqual([ln for ln, _ in found], [3, 4, 5, 6, 7, 8, 9, 11])
        self.assertEqual(found[0][1], "if(a) g();")
        self.assertEqual(found[3][1], "for(int i = 0; i < n; i++) g();")

    def test_statement_attribute_before_a_braced_body_is_clean(self):
        src = d("""
            void f()
            {
              if(a) [[likely]] { g(); }
              else [[unlikely]] { h(); }
              if(a) [[likely]]
                {
                  g();
                }
              else if(b) [[unlikely]] { h(); }
              for(int i = 0; i < n; i++) [[likely]] { g(); }
              while(x) [[unlikely]] { x--; }
              do [[likely]] { x++; } while(x < 3);
              if(a) [[likely]] [[gnu::hot]] { g(); }
              if(a) [[gnu::hot, likely]] { g(); }
              if constexpr(c) [[likely]] { g(); }
            }
            """)
        self.assertEqual(I.unbraced_bodies(src), [])

    def test_statement_attribute_before_an_unbraced_body_is_reported(self):
        src = d("""
            void f()
            {
              if(a) [[likely]] g();
              else [[unlikely]] h();
              for(int i = 0; i < n; i++) [[likely]] g();
              while(x) [[unlikely]] x--;
              do [[likely]] x++; while(x < 3);
              if(a) [[likely]]
                g();
              if(a) [[likely]] v[0] = 1;
            }
            """)
        self.assertEqual(self.lines(src), [3, 4, 5, 6, 7, 8, 10])

    def test_subscript_or_lambda_after_the_condition_is_not_an_attribute(self):
        self.assertEqual(self.lines("void f()\n{\n  if(a) [&] { g(); }();\n}\n"), [3])

    def test_else_if_chain_reports_only_the_unbraced_branch(self):
        self.assertEqual(
            self.lines("void f()\n{\n  if(a) { g(); }\n  else if(b) h();\n}\n"), [4]
        )

    def test_nested_unbraced_statements_are_each_reported(self):
        self.assertEqual(
            self.lines("void f()\n{\n  for(;;)\n    if(a) g();\n}\n"), [3, 4]
        )

    def test_comments_literals_and_macros_are_not_scanned(self):
        src = '// if(a) g();\n/* for(;;) x(); */\nconst char *s = "if(a) g(); else h();";\n#define M(x) if(x) g()\n'
        self.assertEqual(I.unbraced_bodies(src), [])

    def test_identifiers_containing_keywords(self):
        self.assertEqual(
            I.unbraced_bodies(
                "void f()\n{\n  ifx(a);\n  do_it(b);\n  whiled(c);\n  x.else_ = 1;\n}\n"
            ),
            [],
        )


# --------------------------------------------------------------------------- clang-format lookup


def fake_binary(directory, version, name="clang-format"):
    path = os.path.join(directory, name)
    with open(path, "w") as f:
        f.write(f'#!/bin/sh\necho "clang-format version {version}"\n')
    os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)
    return path


class LookupTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)
        self.saved = (os.environ.get("IMTOOL_CLANG_FORMAT"), I.pip_binary, shutil.which)
        self.addCleanup(self.restore)

    def restore(self):
        env, pip, which = self.saved
        if env is None:
            os.environ.pop("IMTOOL_CLANG_FORMAT", None)
        else:
            os.environ["IMTOOL_CLANG_FORMAT"] = env
        I.pip_binary = pip
        shutil.which = which

    def test_order_flag_then_environment_then_pip_then_path(self):
        os.environ["IMTOOL_CLANG_FORMAT"] = "from-env"
        I.pip_binary = lambda: "from-pip"
        shutil.which = lambda name: "from-path"
        self.assertEqual(I.find_binary("from-flag"), "from-flag")
        self.assertEqual(I.find_binary(), "from-env")
        del os.environ["IMTOOL_CLANG_FORMAT"]
        self.assertEqual(I.find_binary(), "from-pip")
        I.pip_binary = lambda: None
        self.assertEqual(I.find_binary(), "from-path")
        shutil.which = lambda name: None
        self.assertIsNone(I.find_binary())

    def test_style_file_names_the_pinned_version(self):
        self.assertIsNotNone(PINNED)
        self.assertRegex(str(PINNED), r"^\d+\.\d+\.\d+$")
        with open(os.path.join(HERE, "requirements.txt")) as f:
            lines = [x.strip() for x in f if x.strip() and not x.startswith("#")]
        # one requirement, pinned to the same version, every continuation line a sha256 hash
        self.assertEqual(lines[0].rstrip("\\").strip(), f"clang-format=={PINNED}")
        self.assertGreater(len(lines), 1)
        for line in lines[1:]:
            self.assertRegex(line, r"^--hash=sha256:[0-9a-f]{64}( \\)?$")

    def test_binary_version_of_a_fake_binary(self):
        self.assertEqual(I.binary_version(fake_binary(self.dir, "1.2.3")), "1.2.3")
        self.assertIsNone(I.binary_version(os.path.join(self.dir, "missing")))

    def run_tool(self, *args, env=None):
        return subprocess.run(
            [sys.executable, TOOL, *args],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
        )

    def test_wrong_version_is_refused_with_the_install_command(self):
        target = os.path.join(self.dir, "a.cpp")
        with open(target, "w") as f:
            f.write("int   x;\n")
        p = self.run_tool("--clang-format", fake_binary(self.dir, "1.2.3"), target)
        self.assertEqual(p.returncode, 2)
        self.assertIn(f"pip install clang-format=={PINNED}", p.stderr)
        self.assertIn("1.2.3", p.stderr)
        with open(target) as f:
            self.assertEqual(f.read(), "int   x;\n")

    def test_missing_binary_is_refused_with_the_install_command(self):
        target = os.path.join(self.dir, "a.cpp")
        with open(target, "w") as f:
            f.write("int x;\n")
        p = self.run_tool("--clang-format", os.path.join(self.dir, "missing"), target)
        self.assertEqual(p.returncode, 2)
        self.assertIn(f"pip install clang-format=={PINNED}", p.stderr)

    def test_environment_variable_is_used(self):
        target = os.path.join(self.dir, "a.cpp")
        with open(target, "w") as f:
            f.write("int x;\n")
        env = dict(os.environ, IMTOOL_CLANG_FORMAT=fake_binary(self.dir, "9.9.9"))
        p = self.run_tool(target, env=env)
        self.assertEqual(p.returncode, 2)
        self.assertIn("9.9.9", p.stderr)

    def test_missing_style_file(self):
        p = self.run_tool(
            "--style-file",
            os.path.join(self.dir, "nope"),
            os.path.join(self.dir, "a.cpp"),
        )
        self.assertEqual(p.returncode, 2)
        self.assertIn("style file not found", p.stderr)


class RefusalTests(unittest.TestCase):
    """main() with a stand-in for clang-format: what is written, printed and returned.

    The binary is a script that reports the pinned version; run_clang_format is replaced, so
    these tests need no clang-format.
    """

    SRC = "int   x;\n"

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)
        self.binary = fake_binary(self.dir, PINNED)
        self.path = os.path.join(self.dir, "a.cpp")
        with open(self.path, "w") as f:
            f.write(self.SRC)
        orig = I.run_clang_format
        self.addCleanup(setattr, I, "run_clang_format", orig)

    def main(self, fake, *args):
        I.run_clang_format = fake
        err, out = io.StringIO(), io.StringIO()
        with contextlib.redirect_stderr(err), contextlib.redirect_stdout(out):
            status = I.main(["--clang-format", self.binary, *args])
        return status, out.getvalue(), err.getvalue()

    def read(self, path):
        with open(path, newline="") as f:
            return f.read()

    @staticmethod
    def collapse(binary, style, text):
        return re.sub(r" +", " ", text)

    def test_whitespace_change_is_written(self):
        status, _out, err = self.main(self.collapse, self.path)
        self.assertEqual((status, err), (0, ""))
        self.assertEqual(self.read(self.path), "int x;\n")
        self.assertEqual(sorted(os.listdir(self.dir)), ["a.cpp", "clang-format"])

    def test_token_change_leaves_the_file_untouched(self):
        def renames(binary, style, text):
            return text.replace("x", "y")

        for args in ((self.path,), ("--check", self.path)):
            status, out, err = self.main(renames, *args)
            self.assertEqual((status, out), (2, ""))
            self.assertIn(f"{self.path}: token sequence would change", err)
            self.assertIn("file left untouched", err)
            self.assertEqual(self.read(self.path), self.SRC)

    def test_merged_words_leave_the_file_untouched(self):
        def merges(binary, style, text):
            return text.replace("int   x", "intx")

        status, _out, err = self.main(merges, self.path)
        self.assertEqual(status, 2)
        self.assertIn("token sequence would change", err)
        self.assertEqual(self.read(self.path), self.SRC)

    def test_no_fixed_point_leaves_the_file_untouched(self):
        def growing(binary, style, text):
            return text + "\n"

        for args in ((self.path,), ("--check", self.path)):
            status, out, err = self.main(growing, *args)
            self.assertEqual((status, out), (2, ""))
            self.assertIn(
                f"{self.path}: clang-format does not reach a fixed point", err
            )
            self.assertEqual(self.read(self.path), self.SRC)

    def test_clang_format_failure_leaves_the_file_untouched(self):
        def fails(binary, style, text):
            raise RuntimeError("clang-format failed: boom")

        status, _out, err = self.main(fails, self.path)
        self.assertEqual(status, 2)
        self.assertIn("boom", err)
        self.assertEqual(self.read(self.path), self.SRC)

    def test_a_refused_file_does_not_stop_the_others(self):
        other = os.path.join(self.dir, "b.cpp")
        with open(other, "w") as f:
            f.write("int   keep;\n")

        def renames_x(binary, style, text):
            return re.sub(r" +", " ", text).replace("x", "y")

        status, _out, err = self.main(renames_x, self.path, other)
        self.assertEqual(status, 2)
        self.assertEqual(self.read(self.path), self.SRC)
        self.assertEqual(self.read(other), "int keep;\n")

    def test_wrong_version_leaves_every_file_untouched(self):
        self.binary = fake_binary(self.dir, "1.2.3", name="other-clang-format")
        status, _out, err = self.main(self.collapse, self.path)
        self.assertEqual(status, 2)
        self.assertIn(f"pinned to {PINNED}", err)
        self.assertEqual(self.read(self.path), self.SRC)

    def test_symbolic_link_is_written_through_to_its_target(self):
        real_dir = os.path.join(self.dir, "real")
        os.mkdir(real_dir)
        target = os.path.join(real_dir, "t.cpp")
        with open(target, "w") as f:
            f.write(self.SRC)
        os.chmod(target, 0o640)
        link = os.path.join(self.dir, "link.cpp")
        try:
            os.symlink(os.path.join("real", "t.cpp"), link)
        except (OSError, NotImplementedError):
            self.skipTest("symbolic links are not available")
        status, _out, err = self.main(self.collapse, link)
        self.assertEqual((status, err), (0, ""))
        self.assertTrue(os.path.islink(link))
        self.assertEqual(os.readlink(link), os.path.join("real", "t.cpp"))
        self.assertEqual(self.read(target), "int x;\n")
        self.assertEqual(stat.S_IMODE(os.stat(target).st_mode), 0o640)
        self.assertEqual(os.listdir(real_dir), ["t.cpp"])

    def test_file_named_like_the_old_temporary_file_is_not_overwritten(self):
        bystander = self.path + ".imtool_tmp"
        with open(bystander, "w") as f:
            f.write("keep me\n")
        status, _out, _err = self.main(self.collapse, self.path)
        self.assertEqual(status, 0)
        self.assertEqual(self.read(bystander), "keep me\n")
        self.assertEqual(self.read(self.path), "int x;\n")

    @unittest.skipIf(
        os.name != "posix" or os.geteuid() == 0,
        "needs a directory the user cannot write to",
    )
    def test_write_failure_is_reported_and_the_other_files_are_still_formatted(self):
        locked = os.path.join(self.dir, "locked")
        os.mkdir(locked)
        first = os.path.join(locked, "first.cpp")
        with open(first, "w") as f:
            f.write(self.SRC)
        os.chmod(locked, 0o555)
        self.addCleanup(os.chmod, locked, 0o755)
        status, _out, err = self.main(self.collapse, first, self.path)
        self.assertEqual(status, 2)
        self.assertIn(f"imtool_format: {first}: ", err)
        self.assertNotIn("Traceback", err)
        self.assertEqual(self.read(first), self.SRC)
        self.assertEqual(os.listdir(locked), ["first.cpp"])
        self.assertEqual(self.read(self.path), "int x;\n")

    def test_missing_file_is_reported_with_status_2(self):
        status, _out, err = self.main(self.collapse, os.path.join(self.dir, "none.cpp"))
        self.assertEqual(status, 2)
        self.assertIn("none.cpp", err)

    def test_byte_order_mark_and_line_endings_are_written_back(self):
        with open(self.path, "w", newline="", encoding="utf-8") as f:
            f.write("﻿#define AREA w * h\r\nint   x;\r\n")
        seen = []

        def collapse(binary, style, text):
            seen.append(text)
            return re.sub(r" +", " ", text)

        status, _out, err = self.main(collapse, self.path)
        self.assertEqual((status, err), (0, ""))
        with open(self.path, newline="", encoding="utf-8") as f:
            self.assertEqual(f.read(), "﻿#define AREA w * h\r\nint x;\r\n")


class RequireBinaryTests(unittest.TestCase):
    """IMTOOL_FORMAT_REQUIRE_BINARY turns the skip of the binary-dependent tests into a failure."""

    # one test that needs no binary, one decorated method and one decorated class
    TARGETS = (
        "LexTests.test_roundtrip",
        "PropertyTests.test_format_twice_equals_once",
        "CommandLineTests",
    )

    def run_tests_without_the_pinned_binary(self, require):
        directory = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, directory)
        env = dict(os.environ, IMTOOL_CLANG_FORMAT=fake_binary(directory, "1.2.3"))
        env.pop("IMTOOL_FORMAT_REQUIRE_BINARY", None)
        if require is not None:
            env["IMTOOL_FORMAT_REQUIRE_BINARY"] = require
        return subprocess.run(
            [sys.executable, os.path.abspath(__file__), *self.TARGETS],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
        )

    def test_without_the_variable_the_tests_skip(self):
        for require in (None, "", "0"):
            p = self.run_tests_without_the_pinned_binary(require)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertRegex(p.stderr, r"OK \(skipped=\d+\)")

    def test_with_the_variable_every_binary_test_fails(self):
        p = self.run_tests_without_the_pinned_binary("1")
        self.assertNotEqual(p.returncode, 0, p.stderr)
        self.assertNotIn("skipped", p.stderr)
        ran = int(re.search(r"Ran (\d+) tests?", p.stderr).group(1))
        self.assertGreater(ran, 2)
        self.assertIn(f"FAILED (failures={ran - 1})", p.stderr)
        self.assertIn(f"clang-format {PINNED} not found", p.stderr)


# --------------------------------------------------------------------------- whole-tool properties

SAMPLE = d("""
    #include "a.hpp"
    #define SQ(x) ((x) * (x))
    namespace ng {
    enum Color { RED = 0, GREEN, BLUE, ALPHA_CHANNEL_WITH_A_LONG_NAME, ANOTHER_ENUMERATOR_WITH_A_LONG_NAME, YET_ANOTHER_ENUMERATOR_WITH_A_LONG_NAME, LAST };
    template<typename T>
    struct Rect {
        Rect(const T& a, const T& b) : p1(a), p2(b) {}
        template<typename U>
        Rect(const Rect<U>& o) : p1(o.p1), p2(o.p2) {}
        Rect& operator=(const Rect& o) { p1 = o.p1; p2 = o.p2; return *this; }
        template<typename U>
        Rect& operator=(const Rect<U>& o) { p1 = o.p1; p2 = o.p2; return *this; }
        T p1, p2;
    };
    class Foo : public Bar {
    public:
        Foo() : m_a(1), m_b(2) {}
        const std::vector<Node*>& nodes() const { return m_nodes; }
        Node* find(int id, std::map<int, Node*>* m, float*);
        void reset() { m_a = 0; m_b = 0; m_nodes.clear(); m_dirty = true; }
    protected:
        int m_a = 0;
        std::vector<Node*> m_nodes;
    };
    }
    void NodeGraph::add(const std::vector<Node*>& nn) {
        const char* s = "a * b // not a comment";
        auto raw = R"(keep   this * as / is
          {  })";
        for (int i = firstNew; i < mNodes.size(); i++) { mNodes[i]->setId(nextId++); }
        if (n) { n->setId(nextId++); mNodes.push_back(n); } else if (q) { foo(); } else { bar(); baz(); qux(); quux(); }
        do { x++; y = x * 2 / z; } while (x < 3);
        switch (x) { case 1: foo(); break; default: break; }
        try { foo(); bar(); } catch (const std::exception& e) { report(e); throw; }
        auto g = [&](int a) { foo(); return a + 1; };
        /* block
           comment */
        // clang-format off
        int   keep  =  a * b;
        // clang-format on
        if (veryLongConditionNumberOne && veryLongConditionNumberTwo && veryLongConditionNumberThree && veryLongConditionNumberFour) { doSomethingQuiteLongHere(argumentOne, argumentTwo); }
        int *q = (int*)ptr; float r = a * *q + b / *q;
    }
    """)

UNBRACED = "void f() {\n    if (a) g();\n    for (int i = 0; i < 3; i++)\n        h(i);\n    while (x) { x--; }\n}\n"


class PropertyTests(Base):
    def test_post_pass_keeps_tokens_on_gnu_shaped_text(self):
        src = d("""
            namespace ng
            {
              enum Color
              {
                RED,
              };
              Node *Graph::find(int id, std::map<int, Node *> *m)
              {
                try
                  {
                    if(a)
                      {
                        return (Node *)m->at(id * 2 / 3);
                      }
                  }
                catch(...)
                  {
                    x = a * b;
                    y = c / d;
                  }
                do
                  {
                    i++;
                  }
                while(i < 3);
                return nullptr;
              }
            }
            """)
        self.assertNotEqual(self.post(src), src)

    def test_post_pass_is_idempotent_without_lambdas(self):
        # (a wrapped lambda brace moves one level out on every pass; clang-format puts it back, so
        # the tool as a whole is idempotent, see test_format_twice_equals_once)
        src = "enum E\n{\n  A,\n};\nNode *f(std::vector<T *> v)\n{\n  if(a)\n    {\n      x = a * b;\n      y = 2;\n    }\n  g();\n  h();\n  return (Node *)v[0];\n}\n"
        once = self.post(src)
        self.assertEqual(self.post(once), once)

    @needs_binary
    def test_format_twice_equals_once(self):
        once = I.format_text(SAMPLE, BINARY, STYLE, LIMIT, WIDTH)
        self.assertEqual(I.format_text(once, BINARY, STYLE, LIMIT, WIDTH), once)

    @needs_binary
    def test_block_after_a_statement_attribute_is_left_where_clang_format_puts_it(self):
        src = "void f(int x) {\n  switch (x) { case 1: [[likely]] { a(); b(); c(); d(); } default: e(); }\n}\n"
        out = I.format_text(src, BINARY, STYLE, LIMIT, WIDTH)
        self.assertEqual(out, I.run_clang_format(BINARY, STYLE, src))
        self.assertIn("    case 1:\n      [[likely]]\n      {\n        a();\n", out)

    @needs_binary
    def test_attributed_control_statements_are_not_reported_after_formatting(self):
        src = "void f(int x) {\n  if (x) [[likely]] { a(); b(); c(); d(); } else [[unlikely]] { a(); b(); c(); d(); }\n}\n"
        out = I.format_text(src, BINARY, STYLE, LIMIT, WIDTH)
        self.assertIn("  if(x) [[likely]]\n    {\n", out)
        self.assertEqual(I.unbraced_bodies(out), [])

    COMMENTS = d("""
        enum E {
          A, // a
          /* block */
          B,
            /* multi
               line */
          C, /* trailing block */
          /* after trailing block */
          D,
        };
        void f() {
          run([&](int a) {
              /* first
                 second */
              g(); // trailing
              /* after a trailing line comment */
              h(); /* trailing block */
                   // aligned
              k(); m();
            });
        }
        """)

    @needs_binary
    def test_block_comments_in_shifted_blocks_converge_and_keep_their_text(self):
        out = I.format_text(self.COMMENTS, BINARY, STYLE, LIMIT, WIDTH)
        self.assertEqual(I.format_text(out, BINARY, STYLE, LIMIT, WIDTH), out)
        self.assertEqual(I.token_signature(self.COMMENTS), I.token_signature(out))
        # the later lines of a block comment are byte-identical
        self.assertIn("/* multi\n       line */\n", out)
        self.assertIn("/* first\n         second */\n", out)
        self.assertIn("enum E\n  {\n    A, // a\n", out)
        self.assertIn("    [&](int a)\n    {\n      /* first\n", out)

    @needs_binary
    def test_header_and_other_extensions_are_formatted_as_cpp(self):
        # clang-format alone takes this for Objective-C and finds no section for it in the
        # style file; the tool always formats as C++
        objc = "@interface Foo : NSObject\n@end\nvoid f(int*p){if(p){g();}}\n"
        out = I.format_text(objc, BINARY, STYLE, LIMIT, WIDTH)
        self.assertIn("void f(int *p)\n{\n  if(p) { g(); }\n}\n", out)

    @needs_binary
    def test_macro_bodies_and_directives_are_left_as_written(self):
        macro = "#define BLOCK(x)   \\\n   if (x) {    \\\n      g( x );  \\\n    }\n"
        src = macro + "#define   SQ(x)    ((x)   *  (x))\nint   y;\n"
        out = I.format_text(src, BINARY, STYLE, LIMIT, WIDTH)
        self.assertEqual(out, macro + "#define SQ(x)    ((x)   *  (x))\nint y;\n")

    @needs_binary
    def test_clang_format_off_region_survives_the_whole_tool(self):
        region = "// clang-format off\nenum   Keep\n{\n  A  =  a * b,\n};\nstd::vector<T *> v;\nif (a)  g( );\n// clang-format on\n"
        out = I.format_text(
            "int   x;\n" + region + "int   y = a * b;\n", BINARY, STYLE, LIMIT, WIDTH
        )
        self.assertEqual(out, "int x;\n" + region + "int y = a*b;\n")

    @needs_binary
    def test_trailing_comment_spacing_is_left_as_written(self):
        src = "int a = 1;   // one\nint bb = 2;       // two\nint c = 3; // three\n"
        self.assertEqual(I.format_text(src, BINARY, STYLE, LIMIT, WIDTH), src)

    @needs_binary
    def test_byte_order_mark_is_kept_and_the_first_directive_is_untouched(self):
        src = "﻿#define AREA w * h\nint   x = a * b;\n"
        self.assertEqual(
            I.format_text(src, BINARY, STYLE, LIMIT, WIDTH),
            "﻿#define AREA w * h\nint x = a*b;\n",
        )

    @needs_binary
    def test_enum_stays_unshifted_when_the_shift_would_pass_the_limit(self):
        name = "A" * (LIMIT - len("   = 0,") - WIDTH + 1)
        src = f"enum E {{ {name} = 0, B }};\n"
        out = I.format_text(src, BINARY, STYLE, LIMIT, WIDTH)
        self.assertEqual(out, f"enum E\n{{\n  {name} = 0,\n  B\n}};\n")
        self.assertEqual(I.format_text(out, BINARY, STYLE, LIMIT, WIDTH), out)
        self.assertEqual(I.token_signature(src), I.token_signature(out))

    @needs_binary
    def test_format_keeps_tokens(self):
        out = I.format_text(SAMPLE, BINARY, STYLE, LIMIT, WIDTH)
        self.assertEqual(strip_ws(SAMPLE), strip_ws(out))
        self.assertEqual(I.token_signature(SAMPLE), I.token_signature(out))

    @needs_binary
    def test_format_produces_the_style(self):
        out = I.format_text(SAMPLE, BINARY, STYLE, LIMIT, WIDTH)
        for want in (
            "namespace ng\n{\n",
            "  enum Color\n    {\n      RED = 0,\n",
            "  template<typename T>\n  struct Rect\n  {\n    Rect(const T &a, const T &b) : p1(a), p2(b) { }\n",
            "    template<typename U>\n    Rect(const Rect<U> &o) : p1(o.p1), p2(o.p2) { }\n",
            "    Rect& operator=(const Rect &o) { p1 = o.p1; p2 = o.p2; return *this; }\n",
            "    template<typename U>\n    Rect& operator=(const Rect<U> &o) { p1 = o.p1; p2 = o.p2; return *this; }\n",
            "  class Foo : public Bar\n  {\n  public:\n",
            "    const std::vector<Node*>& nodes() const { return m_nodes; }\n",
            "    Node* find(int id, std::map<int, Node*> *m, float*);\n",
            "    void reset()\n    {\n      m_a = 0;\n      m_b = 0;\n      m_nodes.clear();\n      m_dirty = true;\n    }\n",
            "void NodeGraph::add(const std::vector<Node*> &nn)\n{\n",
            "  for(int i = firstNew; i < mNodes.size(); i++) { mNodes[i]->setId(nextId++); }\n",
            "  if(n) { n->setId(nextId++); mNodes.push_back(n); }\n  else if(q) { foo(); }\n  else\n    {\n      bar();\n",
            "  do\n    {\n      x++;\n      y = x*2/z;\n    } while(x < 3);\n",
            "  switch(x)\n    {\n    case 1:",
            "  try\n    {\n      foo();\n      bar();\n    }\n  catch(const std::exception &e)\n    {\n      report(e);\n      throw;\n    }\n",
            "  auto g = [&](int a)\n  {\n    foo();\n    return a + 1;\n  };\n",
            "  int   keep  =  a * b;\n",
            "veryLongConditionNumberFour)\n    { doSomethingQuiteLongHere(argumentOne, argumentTwo); }\n",
            "  int *q = (int*)ptr;\n  float r = a * *q + b / *q;\n",
            '  auto raw = R"(keep   this * as / is\n      {  })";\n',
            "#define SQ(x) ((x) * (x))\n",
        ):
            self.assertIn(want, out)


@needs_binary
class CommandLineTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)
        self.path = self.write("a.cpp", SAMPLE)

    def write(self, name, text):
        path = os.path.join(self.dir, name)
        with open(path, "w") as f:
            f.write(text)
        return path

    def read(self, path):
        with open(path) as f:
            return f.read()

    def run_tool(self, *args):
        return subprocess.run(
            [sys.executable, TOOL, "--clang-format", BINARY, *args],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    def test_check_writes_nothing_and_exits_1(self):
        p = self.run_tool("--check", self.path)
        self.assertEqual(p.returncode, 1)
        self.assertEqual(p.stdout.strip(), self.path)
        self.assertEqual(self.read(self.path), SAMPLE)

    def test_in_place_then_check_is_clean(self):
        p = self.run_tool(self.path)
        self.assertEqual((p.returncode, p.stderr), (0, ""))
        formatted = self.read(self.path)
        self.assertNotEqual(formatted, SAMPLE)
        p = self.run_tool("--check", self.path)
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, "", ""))
        self.assertEqual(self.run_tool(self.path).returncode, 0)
        self.assertEqual(self.read(self.path), formatted)

    def test_style_file_option(self):
        self.assertEqual(
            self.run_tool("--style-file", STYLE, "--check", self.path).returncode, 1
        )

    def test_check_reports_unbraced_bodies_as_errors(self):
        path = self.write("u.cpp", UNBRACED)
        p = self.run_tool("--check", path)
        self.assertEqual(p.returncode, 1)
        self.assertIn(
            f"{path}:2: error: control statement without braces: if (a) g();", p.stderr
        )
        self.assertIn(
            f"{path}:3: error: control statement without braces: for (int i = 0; i < 3; i++)",
            p.stderr,
        )
        self.assertEqual(p.stderr.count(": error: "), 2)
        self.assertEqual(self.read(path), UNBRACED)

    def test_check_reports_unbraced_bodies_in_an_already_formatted_file(self):
        path = self.write("u.cpp", UNBRACED)
        self.run_tool("--no-brace-check", path)
        formatted = self.read(path)
        p = self.run_tool("--check", path)
        self.assertEqual((p.returncode, p.stdout), (1, ""))
        self.assertEqual(p.stderr.count(": error: "), 2)
        self.assertEqual(self.read(path), formatted)

    def test_rewrite_warns_about_unbraced_bodies_formats_and_exits_1(self):
        path = self.write("u.cpp", UNBRACED)
        p = self.run_tool(path)
        self.assertEqual(p.returncode, 1)
        formatted = self.read(path)
        self.assertNotEqual(formatted, UNBRACED)
        self.assertEqual(strip_ws(formatted), strip_ws(UNBRACED))
        self.assertEqual(
            p.stderr.count(": warning: control statement without braces: "), 2
        )
        self.assertNotIn(": error: ", p.stderr)
        # line numbers refer to the rewritten file
        prefix = path + ":"
        findings = [x for x in p.stderr.split("\n") if x.startswith(prefix)]
        self.assertEqual(len(findings), 2)
        for line in findings:
            # the path may contain a colon, so the number is read after the known prefix
            number = int(line[len(prefix) :].split(":", 1)[0])
            self.assertTrue(
                formatted.split("\n")[number - 1].strip().startswith(("if(a)", "for("))
            )

    def test_no_brace_check_flag(self):
        path = self.write("u.cpp", UNBRACED)
        p = self.run_tool("--no-brace-check", "--check", path)
        self.assertEqual(
            (p.returncode, p.stderr), (1, "")
        )  # 1 only because the file would change
        p = self.run_tool("--no-brace-check", path)
        self.assertEqual((p.returncode, p.stderr), (0, ""))
        p = self.run_tool("--no-brace-check", "--check", path)
        self.assertEqual((p.returncode, p.stdout, p.stderr), (0, "", ""))


if __name__ == "__main__":
    unittest.main()
