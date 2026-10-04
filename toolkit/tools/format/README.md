# imtool_format

`imtool_format.py` formats C++ into the project layout in one step. It runs clang-format with `imtool.clang-format`, then applies a post-pass for the parts of the layout that clang-format cannot express. The layout is described in the "Layout" section of `toolkit/docs/conventions.md`.

Python 3 standard library only. The one external requirement is the pinned clang-format binary.

## Install clang-format

The style file is fitted to one clang-format version. Its first line names that version, and `requirements.txt` pins the same one.

```
pip install -r toolkit/tools/format/requirements.txt
```

`requirements.txt` lists the sha256 of each 23.1.2 wheel on PyPI, so pip refuses a download that does not match. There are wheels for Linux (x86_64, aarch64 and others), macOS (x86_64, arm64) and Windows.

The tool looks for the binary in this order:

1. `--clang-format BIN`
2. the `IMTOOL_CLANG_FORMAT` environment variable
3. the `clang-format` installed by the pip package in the running Python environment
4. `clang-format` on PATH

If the binary it finds reports another version, the tool exits with status 2 and prints the `pip install` command for the pinned version. Other versions lay out some constructs differently, so the output would not be reproducible.

## Usage

Format files in place:

```
python3 toolkit/tools/format/imtool_format.py src/a.cpp include/a.hpp
```

Check without writing:

```
python3 toolkit/tools/format/imtool_format.py --check src/a.cpp include/a.hpp
```

`--check` prints each file that would change on stdout.

Options:

- `--check`: write nothing.
- `--no-brace-check`: do not report control statements without braces.
- `--style-file F`: use another style file. Default: `imtool.clang-format` next to the script.
- `--clang-format BIN`: use this binary.

Files with any extension are formatted as C++. This includes `.h`, `.cu` and `.cuh`. The tool passes the text to clang-format on stdin with `--assume-filename=input.cpp`, so clang-format does not guess the language from the file name or take a header for Objective-C.

A symbolic link is followed: the tool rewrites the file the link points to and leaves the link in place. The new content is written to a temporary file in the same directory, which then replaces the file, so a failed write leaves the file as it was. The file keeps its permission bits. Other hard links to the file keep the old content.

### Unbraced bodies

The layout requires braces on the bodies of `if`, `else`, `for`, `while` and `do`. The tool does not add them, because it never changes tokens. It reports each unbraced body on stderr. A `[[likely]]` or other attribute between the statement and its braces is allowed.

With `--check` the label is `error:` and the line number refers to the file as it is:

```
src/a.cpp:42: error: control statement without braces: if(n) return;
```

When rewriting, the label is `warning:`, the file is still formatted, and the line number refers to the rewritten file:

```
src/a.cpp:40: warning: control statement without braces: if(n) return;
```

The exit status is 1 in both modes. The label differs, the status does not: a script that formats in place gets status 1 after the files were rewritten. Add the braces by hand, or pass `--no-brace-check`.

### Exit status

- `0`: nothing to report.
- `1`: `--check` found files that would change, or an unbraced body was reported (in either mode, as `error:` or `warning:`).
- `2`: a file could not be read, formatted or written, the binary is missing, or its version is not the pinned one. The other files on the command line are still processed.

## Guarantees

- **Whitespace only.** The tool changes spaces and line breaks. Before it writes a file it compares the token sequence of its output with the input. On any difference it leaves the file untouched and exits with status 2.
- **Idempotent.** The tool repeats clang-format and the post-pass until the output stops changing, normally two rounds. Formatting a formatted file changes nothing.
- **Refuses files that do not converge.** clang-format does not reach a fixed point on some inputs, for example a run of macro calls without semicolons, each followed by a comment. The tool leaves such a file untouched, names it on stderr and exits with status 2. Put the region between `// clang-format off` and `// clang-format on` and run again.
- **Protected regions.** Lines between `// clang-format off` and `// clang-format on` are not changed, by clang-format or by the post-pass. The `/* clang-format off */` form works too.
- **Not scanned.** The post-pass does not change preprocessor lines, string literals (raw strings included), character literals, or the text of comments. It can change the indentation of a line that starts with a comment. The second and later lines of a block comment stay as written.
- **Byte order mark.** A UTF-8 byte order mark at the start of a file is kept.

## What it does not do

- It does not add braces, rename identifiers, sort includes, or re-wrap comments.
- It does not format macro bodies. clang-format normalizes the spaces between `#define` and the macro name; the body, including a body continued with backslashes, is left as written.
- It does not keep hand alignment of `=`, names, arguments or tables. It does not create alignment either.
- It does not move trailing comments. Their spacing is left as written.
- It does not choose break points in long expressions the way a person would. clang-format decides them.

## What the post-pass does

clang-format with the style file produces most of the layout. The post-pass adds:

- **Braces.** Indents `enum` braces one level. Moves a wrapped lambda brace one level out. Joins `}` and `while(..);` at the end of a `do` body.
  - An `enum` is left at the column clang-format gives it when indenting it would move one of its lines past the column limit. The limit takes precedence over the brace rule.
  - A line inside an `enum` that holds only a comment stays at the column it was written at.
- **Bodies.** Puts a function body or an `if` / `else` / `for` / `while` body of one to three statements on the line of its signature or statement when the line fits in 140 columns. Otherwise puts the braced body alone on the next line when that fits. Otherwise leaves the block expanded. A body with a comment, a preprocessor line or a nested block is never joined.
- **Pointers.** Removes the space before `*` and `&` that have no name after them: `<T*>`, `(T*)`, `f(int*)`. Binds a return type left: `T* f()`. A variable initialized with parentheses keeps the `*` or `&` on the name: `T *p(nullptr);`. `T *f(a);` and `T *p(a);` look the same, so the tool reads the parentheses: it takes them for parameters when they are empty, when something other than `;` or `,` follows them, or when they show a declaration (`int n`, `const T&`, `Node *n`, `...`). Otherwise it takes them for an initializer. A declaration whose only parameters are unnamed class types, such as `Node *find(Key);`, is therefore left with the `*` on the name.
- **Operators.** Removes the spaces around binary `*`, `/` and `%`.

## How the config was derived

A set of survey and fitting scripts did this. They are not part of this directory yet.

1. A survey counted the layout habits in 739 hand-formatted files (181,297 lines, 12 projects).
2. The owner's rules were fixed in the config. Options that would change tokens were fixed to their safe value.
3. Every other option was set to the value that changes the fewest corpus lines. Where the survey counts and the line measurement disagreed, the survey decided; the fit log marks each such option.
4. The owner ruled on the points where the corpus was mixed: indented `try` / `catch` braces, bodies of up to three statements on one line, no alignment, `A a { x, y };`, and reporting unbraced bodies instead of adding braces.

Measured on that corpus with clang-format 23.1.2:

| | Lines changed | Ignoring indentation | Ignoring all whitespace |
|---|---|---|---|
| Config alone | 33.1% | 26.7% | 11.5% |
| Config and post-pass, over the 732 files the tool formats | 32.0% | 26.4% | 12.7% |

The tool refuses 7 of the 739 corpus files: 6 copies of one header on which clang-format does not converge, and one file where clang-format drops a stray backslash.

The breakdown of the "Lines changed" column was measured over all 739 files, with the 7 refused files counted as unchanged. On that basis the tool changes 30.9% of the 181,297 corpus lines:

- 11.3 points are lines the layout normalizes on purpose, such as whitespace on blank lines and missing spaces around operators.
- 15.7 points are hand layout that no rule predicts: alignment, break points, tables.
- 4.0 points are layouts a further rule could handle.

## Tests

```
python3 -m unittest discover -s toolkit/tools/format -p 'test_*.py'
```

Tests that run clang-format are skipped when the pinned version is not found. With `IMTOOL_FORMAT_REQUIRE_BINARY=1` in the environment they fail instead. The CI workflow sets it, so the job cannot pass without the pinned binary.
