#!/usr/bin/env python3
"""Insert or refresh the Quick Navigation block in each reference doc.

This is a maintenance script for skill authors — not for end users. Run it
after editing any reference whose section structure or line numbers have
shifted, so the TOC line ranges in the `<!-- QUICK_NAV_BEGIN -->` block
stay accurate. Idempotent: re-running on an unchanged tree is a no-op.

Usage (from anywhere):

    python3 skills/imgui-cpp-development/scripts/refresh-quicknav.py \\
        skills/imgui-cpp-development/references/

If invoked with no arg, defaults to the references directory relative to
this script's location, so editing the skill from the repo root just works:

    python3 skills/imgui-cpp-development/scripts/refresh-quicknav.py

The doc list below covers references that benefit from a TOC (>~150 lines).
Smaller references are left as-is — their tier-guidance opener already
gives an agent enough orientation.
"""
from __future__ import annotations
import re
import sys
from pathlib import Path

DOCS = [
    "styling-fonts-dpi.md", "widget-recipes.md", "tables.md",
    "changelog-1.92.x.md", "testing.md", "layout-and-sizing.md",
    "custom-widgets.md", "modals-and-popups.md", "docking-and-viewports.md",
    "backends/opengl3-glfw.md",
]

MARKER_BEGIN = "<!-- QUICK_NAV_BEGIN -->"
MARKER_END = "<!-- QUICK_NAV_END -->"


def build_toc(lines):
    sections = []
    for idx, line in enumerate(lines, start=1):
        m = re.match(r"^## (.+)$", line)
        if m:
            sections.append((idx, m.group(1).strip()))
    if not sections:
        return None
    out = [MARKER_BEGIN]
    out.append(
        "> **Quick navigation** (jump to a section instead of loading the whole "
        "file - `Read offset=N limit=M`):"
    )
    out.append(">")
    for i, (start, title) in enumerate(sections):
        end = sections[i + 1][0] - 1 if i + 1 < len(sections) else len(lines)
        out.append(f"> - L{start:>4}-{end:<4} {title}")
    out.append(MARKER_END)
    out.append("")
    return "\n".join(out)


def insert_or_replace(text: str) -> str:
    lines = text.splitlines(keepends=False)
    toc_block = build_toc(lines)
    if toc_block is None:
        return text

    if MARKER_BEGIN in text and MARKER_END in text:
        return re.sub(
            re.escape(MARKER_BEGIN) + ".*?" + re.escape(MARKER_END) + r"\n?",
            toc_block + "\n",
            text,
            count=1,
            flags=re.DOTALL,
        )

    m = re.search(r"^> \*\*Load this file when:\*\*", text, re.MULTILINE)
    if m is None:
        m_h1 = re.search(r"^# .+\n", text, re.MULTILINE)
        if m_h1 is None:
            return toc_block + "\n" + text
        insert_at = m_h1.end()
        return text[:insert_at] + "\n" + toc_block + "\n" + text[insert_at:]

    after = text[m.start():]
    body_offset = 0
    seen_quote = False
    for line in after.splitlines(keepends=True):
        body_offset += len(line)
        if line.startswith(">"):
            seen_quote = True
            continue
        if seen_quote and line.strip() == "":
            break
    insert_at = m.start() + body_offset
    return text[:insert_at] + toc_block + "\n" + text[insert_at:]


def main():
    if len(sys.argv) > 1:
        root = Path(sys.argv[1])
    else:
        # Default: ../references relative to this script's directory.
        root = Path(__file__).resolve().parent.parent / "references"

    if not root.is_dir():
        print(f"error: references dir not found at {root}", file=sys.stderr)
        sys.exit(1)

    for rel in DOCS:
        p = root / rel
        if not p.is_file():
            print(f"  SKIP {rel}: not found at {p}")
            continue
        original = p.read_text()
        updated = insert_or_replace(original)
        if updated == original:
            print(f"  no change to {rel}")
            continue
        p.write_text(updated)
        section_count = updated.count("> - L")
        print(f"  TOC added/refreshed in {rel} ({section_count} sections)")


if __name__ == "__main__":
    main()
