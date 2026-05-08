---
description: Audit current file (or current branch's diff) for known Dear ImGui pitfalls — unpaired Begin/End, ID-stack collisions, scrollwindow growth, etc.
argument-hint: "[file-or-glob]"
---

Audit ImGui code for known pitfalls. Defaults to the file currently being edited; pass a path or glob to audit a specific scope; pass `branch` to audit the diff of the current branch against `main`.

Steps:

1. Invoke the `imgui-cpp-development` skill.
2. Determine the audit scope:
   - If `$ARGUMENTS` is empty and there's a "currently focused file" in the conversation, use that.
   - If `$ARGUMENTS` is `branch`, run `git diff main...HEAD --name-only -- '*.cpp' '*.h' '*.hpp'` and audit each file in the result.
   - Otherwise treat `$ARGUMENTS` as a file path or glob; expand with `find`.
3. Run `bash $CLAUDE_PLUGIN_ROOT/skills/imgui-cpp-development/scripts/locate-imgui.sh --json` to confirm an ImGui copy is in scope. If none found, abort with a friendly message — there's nothing to audit against.
4. For each in-scope file, run the bundled lint + pair scripts:
   ```bash
   bash $CLAUDE_PLUGIN_ROOT/skills/imgui-cpp-development/scripts/imgui-pair.sh <file>
   bash $CLAUDE_PLUGIN_ROOT/skills/imgui-cpp-development/scripts/imgui-lint.sh <file>
   ```
5. Aggregate findings. For each issue:
   - Quote the offending line with file:line.
   - Identify which pitfall in `references/pitfalls-catalog.md` it matches.
   - Recommend the fix from the linked deep-dive doc.
6. Summarize: number of issues by area (ID stack / Begin-End pairing / sizing / etc.). Don't auto-apply fixes — present them and let the user pick which to take.

If the audit is clean, say so explicitly. Don't manufacture issues to seem productive.
