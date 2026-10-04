# Re-running the survey and the fit

These scripts derived `../imtool.clang-format` and the post-pass rules from a corpus of hand-formatted C++. They are not needed to format code. Run them again when the corpus changes or when the pinned clang-format version changes.

All scripts read the corpus and write nothing into it. Python 3 standard library only. `diff` must be on PATH.

## Corpus

A corpus is a directory with one sub-directory per project. Files with the extensions `.hpp .cpp .h .cu .cuh` are used.

Left out automatically:

- directories named `lib libs third_party imgui vendor build ext external deps`
- well-known third-party files (`stb_*`, `json.hpp`, `imgui*`, `catch.hpp`, ...)
- generated version headers
- the path prefixes in `common.DEFAULT_EXCLUDE`

`DEFAULT_EXCLUDE` names directories of the original corpus (`counter-example-gui_cpp`, `_my-prior-attempts`, `vk-test-premake`, and two third-party trees under `19-logos/old`). For another corpus, pass `--exclude PREFIX` once per prefix; that replaces the default list.

List what would be used:

```
python3 common.py --corpus CORPUS
```

## Survey

```
python3 survey.py --corpus CORPUS > survey.txt
```

Counts, per project, what the code does: brace placement and indentation per construct, body forms by length and statement count, pointer binding, spacing, alignment, includes, blank lines. The style document quotes these counts.

## Fit

```
python3 fit_config.py --corpus CORPUS --clang-format BIN --out imtool.clang-format --log fit_log.txt
```

`--start-config FILE` starts from an existing `.clang-format`. `--passes N` sets the number of passes over the option list (default 2).

The script applies the fixed rules, then tries each candidate value of each remaining option and keeps the value that changes the fewest corpus lines. The log records every value tried and its share.

Three kinds of option are not decided by the measurement alone:

- `FIXED_RULES`: the owner's rules. Never searched.
- `FIXED_TOKENS`: values that would add, remove or reorder tokens. Never searched.
- `SURVEY_DECIDES`: the survey count wins over the measurement. The line metric counts a wrongly joined block as several changed lines and a wrongly split one as one line, so it favours expanded forms.

A difference under 0.2 percentage points is logged as a coin-flip. The survey preference wins it, then the start-config value, then the current value, then the first listed candidate.

The first line of the written config names the clang-format version. `imtool_format.py` refuses any other version. After a re-fit with a new version, update `../requirements.txt` to the same version.

## Measure

```
python3 measure.py --corpus CORPUS [--clang-format BIN] [--style FILE] [--post]
```

Prints the share of lines changed per project: exact, ignoring leading indentation, ignoring all whitespace. `--post` adds the post-pass.

## Full report

```
python3 results.py --corpus CORPUS [--library DIR ...] [--work DIR] [--baseline-config FILE] > results.txt
```

Copies the corpus files (and each `--library` tree) under `--work`, runs `imtool_format.py` on the copies through its command line, and reports:

- shares of lines changed
- token identity before and after
- idempotence (a second run changes nothing)
- the files the tool refused
- unbraced-body findings
- the effect of each post-pass feature
- a categorized list of what the tool still changes

`--excerpt PATH::REGEX` prints a before / after excerpt of a library file, starting at the first line that matches.

Without `--clang-format`, `measure.py` and `results.py` find the binary the way `imtool_format.py` does.
