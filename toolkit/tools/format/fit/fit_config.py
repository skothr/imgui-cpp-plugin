#!/usr/bin/env python3
"""Fit a clang-format config to a hand-formatted corpus by measurement.

usage: fit_config.py --corpus DIR --clang-format BIN --out FILE --log FILE
                     [--start-config FILE] [--passes 2]

Start: the options of --start-config (an existing .clang-format; optional).  Then the owner's
rules and the token-preservation constraints are applied; these are never searched.  Every
remaining option is chosen by coordinate descent: for each candidate value the share of corpus
lines clang-format would change is measured, and the minimum is kept.

A difference under 0.2 percentage points is a coin-flip: the survey preference wins if there is
one, else the start-config value, else the current value, else the first listed candidate.

Options in SURVEY_DECIDES keep the survey value even when another value measures lower.  The
line metric counts a wrongly joined block as several changed lines and a wrongly split one as
one, so it leans towards expanded forms; those options are decided by counting constructs.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

# The scripts import each other by their path from the repository root, so that the imports
# resolve the same way for Python and for a type checker run from the root.
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from toolkit.tools.format.fit import common, measure  # noqa: E402

COIN = 0.2

# ---- never searched: the owner's rules and rulings
FIXED_RULES = {
    "ColumnLimit": ("140", "lines at most 140 columns"),
    "IndentWidth": ("2", "2-space indent"),
    "TabWidth": ("2", "2-space indent"),
    "UseTab": ("Never", "no tabs"),
    "BreakBeforeBraces": ("Custom", "brace rules below"),
    "BraceWrapping.AfterFunction": ("true", "function brace on its own line"),
    "BraceWrapping.AfterClass": ("true", "class brace on its own line"),
    "BraceWrapping.AfterStruct": ("true", "struct brace on its own line"),
    "BraceWrapping.BeforeCatch": ("true", "catch starts its own line, brace below it"),
    "BraceWrapping.AfterControlStatement": ("Always", "control braces on their own line"),
    "BraceWrapping.BeforeElse": ("true", "else on its own line (survey: 3192 of 3411)"),
    "BraceWrapping.BeforeWhile": ("true", "needed for an indented do brace (survey: 4 of 4); the post-pass re-joins '} while(..);'"),
    "BraceWrapping.IndentBraces": ("true", "GNU layout: braces of if / for / while / do / switch / try / catch indented one level (try / catch by the owner's ruling; survey: 12 of 13)"),
    "SpaceBeforeParens": ("Never", "if( for( while( switch( and f("),
    "AllowShortBlocksOnASingleLine": ("Always", "a short braced body stays on the statement line"),
    "AllowShortIfStatementsOnASingleLine": ("AllIfsAndElse", "same"),
    "AllowShortLoopsOnASingleLine": ("true", "same"),
    "Cpp11BracedListStyle": ("false", "spaces inside initializer braces: A a { x, y };"),
    "SpaceBeforeCpp11BracedList": ("true", "space before an initializer brace: A a { x, y }; (owner's ruling; survey: 537 with, 1493 without)"),
    "PointerAlignment": ("Right", "T *x, const T &x"),
    "DerivePointerAlignment": ("false", "same"),
    "SpaceAfterTemplateKeyword": ("false", "template<typename T>"),
    "AlignConsecutiveAssignments": ("None", "owner's ruling: no '=' alignment"),
    "AlignConsecutiveDeclarations": ("None", "owner's ruling: no declaration alignment"),
    "AlignConsecutiveMacros": ("None", "owner's ruling: AlignConsecutive* off"),
    "AlignConsecutiveBitFields": ("None", "owner's ruling: AlignConsecutive* off"),
    "AlignConsecutiveShortCaseStatements": ("{Enabled: false}", "owner's ruling: AlignConsecutive* off"),
}
# ---- never searched: any other value adds, removes or reorders tokens
FIXED_TOKENS = {
    "SortIncludes": ("false", "reorders lines; survey: 653 of 908 include groups are not sorted"),
    "SortUsingDeclarations": ("false", "reorders lines"),
    "FixNamespaceComments": ("false", "inserts comments"),
    "BreakStringLiterals": ("false", "splits string literals into several tokens"),
    "ReflowComments": ("false", "rewrites comment text"),
    "SkipMacroDefinitionBody": ("true", "formatting a macro body adds and removes backslash line continuations (clang-format 18+)"),
}

# ---- searched, but the survey value is kept even when another value measures lower
SURVEY_DECIDES = {
    "AllowShortFunctionsOnASingleLine": "single-statement functions that fit are written on one line: 1955 of 2266 at namespace scope, 2223 of 2346 in class bodies",
    "BraceWrapping.BeforeLambdaBody": "expanded lambda bodies: brace on its own line 69, on the signature line 8",
    "AllowShortRecordOnASingleLine": "struct bodies closed on the declaration line: 592 of 982",
    "NamespaceIndentation": "namespace bodies indented one level: 239 of 240",
    "AccessModifierOffset": "access specifiers at the class column: 746 of 747",
    "BreakConstructorInitializers": "initializers packed on one line: 566 of 624 lists with several initializers; leading commas: 0 (BeforeComma puts every initializer on its own line)",
    "ConstructorInitializerIndentWidth": "colon line indented one level: 449 of 518",
    "AlignAfterOpenBracket": "wrapped arguments aligned after the open parenthesis: 1568 of 2149 continuation lines",
}

# ---- searched: (option, candidate values as YAML text, survey preference or None)
OPTIONS = [
    ("NamespaceIndentation", ["None", "Inner", "All"], "All"),
    ("AccessModifierOffset", ["0", "-2", "-1"], "-2"),
    ("IndentCaseLabels", ["false", "true"], "false"),
    ("IndentCaseBlocks", ["false", "true"], None),
    ("BraceWrapping.AfterCaseLabel", ["true", "false"], "true"),
    ("BraceWrapping.AfterEnum", ["false", "true"], "true"),
    ("BraceWrapping.AfterNamespace", ["true", "false"], "true"),
    ("BraceWrapping.AfterUnion", ["true", "false"], None),
    ("BraceWrapping.AfterExternBlock", ["false", "true"], None),
    ("BraceWrapping.BeforeLambdaBody", ["false", "true"], "true"),
    ("BraceWrapping.SplitEmptyFunction", ["false", "true"], "false"),
    ("BraceWrapping.SplitEmptyRecord", ["false", "true"], None),
    ("BraceWrapping.SplitEmptyNamespace", ["false", "true"], None),
    ("AllowShortFunctionsOnASingleLine", ["All", "Inline", "InlineOnly", "Empty", "None"], "All"),
    ("AllowShortCaseLabelsOnASingleLine", ["true", "false"], "true"),
    ("AllowShortLambdasOnASingleLine", ["All", "Inline", "Empty", "None"], "All"),
    ("AllowShortEnumsOnASingleLine", ["true", "false"], None),
    ("AllowShortRecordOnASingleLine", ["EmptyAndAttached", "Never", "Empty", "Always"], "Always"),
    ("AllowShortNamespacesOnASingleLine", ["false", "true"], None),
    ("SpaceInEmptyBlock", ["false", "true"], "true"),
    ("SpaceInEmptyBraces", ["Never", "Block", "Always"], "Block"),
    ("SpaceAfterCStyleCast", ["false", "true"], "false"),
    ("SpaceAfterLogicalNot", ["false", "true"], None),
    ("SpaceBeforeRangeBasedForLoopColon", ["true", "false"], None),
    ("SpaceBeforeCtorInitializerColon", ["true", "false"], None),
    ("SpaceBeforeInheritanceColon", ["true", "false"], None),
    ("SpacesBeforeTrailingComments", ["1", "2"], None),
    ("SpacesInAngles", ["false", "true"], "false"),
    ("SpacesInContainerLiterals", ["true", "false"], None),
    ("BitFieldColonSpacing", ["Both", "None", "Before", "After"], None),
    ("AlignTrailingComments", ["true", "false", "{Kind: Leave}", "{Kind: Always, OverEmptyLines: 1}"], None),
    ("AlignOperands", ["AlignAfterOperator", "Align", "DontAlign"], None),
    ("AlignAfterOpenBracket", ["Align", "DontAlign", "AlwaysBreak"], "Align"),
    ("PenaltyBreakBeforeFirstCallParameter", ["19", "100", "1000"], None),
    ("AlignEscapedNewlines", ["Left", "Right", "DontAlign"], None),
    ("AlignArrayOfStructures", ["None", "Left", "Right"], None),
    ("AlwaysBreakTemplateDeclarations", ["Yes", "MultiLine", "No"], None),
    ("BreakTemplateDeclarations", ["Yes", "MultiLine", "No", "Leave"], None),
    ("BinPackArguments", ["true", "false"], None),
    ("BinPackParameters", ["true", "false"], None),
    ("AllowAllArgumentsOnNextLine", ["true", "false"], None),
    ("AllowAllParametersOfDeclarationOnNextLine", ["true", "false"], None),
    ("BreakBeforeBinaryOperators", ["None", "NonAssignment", "All"], None),
    ("BreakBeforeTernaryOperators", ["true", "false"], None),
    ("BreakConstructorInitializers", ["BeforeComma", "BeforeColon", "AfterColon"], "BeforeColon"),
    ("PackConstructorInitializers", ["BinPack", "Never", "CurrentLine", "NextLine"], None),
    ("ConstructorInitializerIndentWidth", ["4", "2", "0"], "2"),
    ("BreakInheritanceList", ["BeforeColon", "BeforeComma", "AfterColon"], None),
    ("ContinuationIndentWidth", ["2", "4"], None),
    ("IndentWrappedFunctionNames", ["false", "true"], None),
    ("IndentPPDirectives", ["None", "AfterHash", "BeforeHash"], None),
    ("LambdaBodyIndentation", ["Signature", "OuterScope"], None),
    ("MaxEmptyLinesToKeep", ["2", "1", "3", "4"], None),
    ("KeepEmptyLinesAtTheStartOfBlocks", ["true", "false"], None),
    ("EmptyLineBeforeAccessModifier", ["LogicalBlock", "Leave", "Never", "Always"], None),
    ("EmptyLineAfterAccessModifier", ["Never", "Leave", "Always"], None),
    ("CompactNamespaces", ["false", "true"], None),
    ("PenaltyReturnTypeOnItsOwnLine", ["60", "200", "1000"], None),
    ("PenaltyBreakAssignment", ["2", "20", "100"], None),
    ("PenaltyExcessCharacter", ["1000000", "100"], None),
    ("Standard", ["c++20", "Latest"], None),
]
# measured for the record against the final config, never adopted
INFORMATIONAL = [
    ("ColumnLimit", ["140", "0", "120", "160"], "the rule fixes 140; 0 makes clang-format keep the input's line breaks"),
    ("PointerAlignment", ["Right", "Left"], "the rule fixes Right"),
    ("SpaceBeforeParens", ["Never", "ControlStatements"], "the rule fixes Never"),
    ("BraceWrapping.IndentBraces", ["true", "false"], "the rule fixes true"),
    ("Cpp11BracedListStyle", ["false", "true"], "the rule fixes false"),
    ("SpaceBeforeCpp11BracedList", ["true", "false"], "the ruling fixes true"),
    ("AlignConsecutiveAssignments", ["None", "Consecutive"], "the ruling fixes None"),
    ("AlignConsecutiveDeclarations", ["None", "Consecutive"], "the ruling fixes None"),
    ("AlignConsecutiveShortCaseStatements", ["{Enabled: false}", "{Enabled: true}"], "the ruling fixes off"),
]
# deprecated spellings a newer clang-format still accepts next to their replacement
ALIASES = [("AlwaysBreakTemplateDeclarations", "BreakTemplateDeclarations"), ("SpaceInEmptyBlock", "SpaceInEmptyBraces")]


def parse_simple_yaml(path):
    """Top-level 'Key: value' pairs and one nested block (BraceWrapping) -> flat dict with dotted keys."""
    cfg = {}
    parent = None
    for raw in common.read(path).split("\n"):
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip() or line.strip() in ("---", "..."):
            continue
        m = re.match(r"^(\s*)([A-Za-z0-9]+):\s*(.*)$", line)
        if not m:
            continue
        indent, key, val = m.groups()
        if indent and parent:
            cfg[parent + "." + key] = val
        elif val == "":
            parent = key
        else:
            parent = None
            cfg[key] = val
    return cfg


def dump_yaml(cfg, header=None):
    out = []
    if header:
        out += ["# " + h for h in header]
    nested = {}
    for k, v in cfg.items():
        if "." in k:
            p, c = k.split(".", 1)
            nested.setdefault(p, []).append((c, v))
    done = set()
    for k, v in cfg.items():
        if "." in k:
            p = k.split(".", 1)[0]
            if p not in done:
                done.add(p)
                out.append(p + ":")
                out += [f"  {c}: {cv}" for c, cv in nested[p]]
        else:
            out.append(f"{k}: {v}")
    return "\n".join(out) + "\n"


class Fitter:
    def __init__(self, binary, root, files, log, tmpdir):
        self.binary = binary
        self.root = root
        self.files = files
        self.log = log
        self.tmp = os.path.join(tmpdir, "trial.clang-format")
        self.cache = {}

    def say(self, s=""):
        print(s)
        self.log.write(s + "\n")
        self.log.flush()

    def accepts(self, cfg):
        """True if this clang-format reads the config without an error."""
        common.write(self.tmp, dump_yaml(cfg))
        p = subprocess.run([self.binary, "--style=file:" + self.tmp, "--assume-filename=x.cpp"], input=b"int x;\n", stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return p.returncode == 0 and b"error" not in p.stderr

    def share(self, cfg):
        """Share (percent) of corpus lines changed, or None if this clang-format rejects the config or crashes."""
        text = dump_yaml(cfg)
        if text in self.cache:
            return self.cache[text]
        value = None
        if self.accepts(cfg):
            try:
                res = measure.measure(self.binary, self.tmp, self.root, self.files, post=False, modes=("exact",))
                value = 100.0 * res["ALL"]["exact"] / res["ALL"]["lines"]
            except RuntimeError:
                value = None  # clang-format crashed on a corpus file with this value
        self.cache[text] = value
        return value


def show(results):
    return "  ".join(f"{v}={'unsupported' if s is None else f'{s:.2f}%'}" for v, s in results)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    common.add_corpus_arguments(ap)
    ap.add_argument("--clang-format", dest="binary", required=True, help="clang-format binary to fit for")
    ap.add_argument("--out", required=True, help="where to write the fitted config")
    ap.add_argument("--log", required=True, help="where to write the log of every value tried")
    ap.add_argument("--start-config", help="existing .clang-format to start from (optional)")
    ap.add_argument("--passes", type=int, default=2)
    a = ap.parse_args()
    root, files = common.corpus_files(a)
    version = common.binary_version(a.binary)
    with open(a.log, "w") as log, tempfile.TemporaryDirectory() as tmpdir:
        F = Fitter(a.binary, root, files, log, tmpdir)
        F.say(f"# fit_config.py: clang-format version {version}")
        F.say(f"# corpus: {len(files)} files; metric: share of original lines a line diff changes (clang-format alone, no post-pass)")
        start_cfg = parse_simple_yaml(a.start_config) if a.start_config else {}
        base = F.share(start_cfg)
        F.say(f"start config unchanged: {'rejected by this version' if base is None else f'{base:.2f}%'}")
        cfg = dict(start_cfg)
        F.say("")
        F.say("## fixed (not searched)")
        for table in (FIXED_RULES, FIXED_TOKENS):
            for k, (v, why) in table.items():
                old = cfg.get(k)
                cfg[k] = v
                F.say(f"  {k}: {v}   [{why}]" + (f"   (start config: {old})" if old not in (None, v) else ""))
        # options this clang-format version does not accept are dropped from the start config
        for k in list(cfg):
            if not F.accepts({k: cfg[k]}):
                F.say(f"  dropped {k}: not accepted by this version")
                del cfg[k]
        start = F.share(cfg)
        if start is None:
            F.say("start config rejected by clang-format; aborting")
            sys.exit(1)
        F.say(f"after fixed rules: {start:.2f}%")
        coin_flips = {}
        for pass_no in range(1, a.passes + 1):
            F.say("")
            F.say(f"## pass {pass_no}")
            changed = False
            for name, values, pref in OPTIONS:
                current = cfg.get(name)
                results = []
                for v in values:
                    trial = dict(cfg)
                    trial[name] = v
                    results.append((v, F.share(trial)))
                ok = [(v, s) for v, s in results if s is not None]
                if not ok:
                    F.say(f"{name}: {show(results)}  -> not supported by this version, left out")
                    cfg.pop(name, None)
                    continue
                best_v, best_s = min(ok, key=lambda x: x[1])
                near = [v for v, s in ok if s - best_s < COIN]
                note = ""
                if len(near) > 1:
                    start_v = start_cfg.get(name)
                    if pref in near:
                        pick, why = pref, "survey"
                    elif name in SURVEY_DECIDES and pref in [v for v, _ in ok]:
                        pick, why = pref, "survey decides: " + SURVEY_DECIDES[name]
                    elif start_v in near:
                        pick, why = start_v, "start-config value"
                    elif current in near:
                        pick, why = current, "current value"
                    else:
                        pick, why = near[0], "first listed candidate"
                    note = f"  COIN-FLIP (<{COIN}pp among {', '.join(near)}): took {pick} ({why})"
                    coin_flips[name] = note.strip()
                    best_v = pick
                else:
                    coin_flips.pop(name, None)
                    if pref is not None and pref != best_v:
                        if name in SURVEY_DECIDES and pref in [v for v, _ in ok]:
                            note = f"  SURVEY DECIDES: measurement prefers {best_v}, took {pref} ({SURVEY_DECIDES[name]})"
                            best_v = pref
                        else:
                            note = f"  NOTE: survey prefers {pref}, measurement prefers {best_v}"
                if best_v != current:
                    changed = True
                cfg[name] = best_v
                F.say(f"{name}: {show(results)}  -> {best_v}{note}")
            after = F.share(cfg)
            assert after is not None
            F.say(f"after pass {pass_no}: {after:.2f}%")
            if not changed:
                F.say("no option changed in this pass; converged")
                break
        final = F.share(cfg)
        assert final is not None
        for old, new in ALIASES:
            if old in cfg and new in cfg:
                trial = {k: v for k, v in cfg.items() if k != old}
                if F.share(trial) == final:
                    cfg = trial
                    F.say(f"removed {old}: deprecated spelling of {new}; share unchanged")
        F.say("")
        F.say("## informational (measured against the final config, never adopted)")
        for name, values, why in INFORMATIONAL:
            results = []
            for v in values:
                trial = dict(cfg)
                trial[name] = v
                results.append((v, F.share(trial)))
            F.say(f"{name}: {show(results)}   [{why}]")
        F.say("")
        F.say("## coin-flips (difference under 0.2 percentage points)")
        for k, v in coin_flips.items():
            F.say(f"  {k}: {v}")
        F.say("")
        F.say(f"final: {final:.2f}%" + ("" if base is None else f"  (start config: {base:.2f}%)"))
        header = [
            f"clang-format version {version}  (pinned: imtool_format.py refuses any other version)",
            "Generated by fit/fit_config.py.  Fixed entries come from the owner's rules; the rest were chosen by",
            "measured churn on the hand-formatted corpus.  Use through imtool_format.py: clang-format alone",
            "cannot produce the full style.",
        ]
        common.write(a.out, dump_yaml(cfg, header))
        F.say(f"wrote {os.path.basename(a.out)}")


if __name__ == "__main__":
    main()
