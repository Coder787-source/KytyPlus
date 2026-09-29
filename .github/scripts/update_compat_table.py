#!/usr/bin/env python3
"""Update COMPATIBILITY.md from a closed [Compatibility report] issue.

Triggered only for issues closed as "completed" with the `compatibility` label
(see .github/workflows/compat-table-update.yml) — a maintainer closing the
issue is the review step; this script just saves the manual markdown editing.
"""
import os
import re
import sys

# The table lives at the repository root. It used to live under docs/, and the
# rename left this script and the workflow pointing at a path that no longer
# existed, so the job failed at `git add` on every run.
#
# Resolve against the repository root rather than the current directory so the
# script also works when invoked from a workflow step or a subdirectory.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
COMPAT_PATH = os.path.join(REPO_ROOT, "COMPATIBILITY.md")

PLACEHOLDER_ROW = (
    "| _No reports yet — be the first to "
    "[file one](https://github.com/Coder787-source/KytyPlus/issues/new?template=compatibility.yml)._ "
    "| | | | |"
)


def parse_issue_fields(body: str) -> dict:
    """GitHub issue-forms render each field as '### Label\\n\\nvalue\\n\\n'."""
    fields = {}
    # Split on "### " headers; first chunk (before first header) is discarded.
    parts = re.split(r"^### +(.+?) *$", body, flags=re.MULTILINE)
    # parts = [preamble, label1, body1, label2, body2, ...]
    for i in range(1, len(parts), 2):
        label = parts[i].strip()
        value = parts[i + 1].strip() if i + 1 < len(parts) else ""
        fields[label] = value
    return fields


def split_title_id(game_field: str) -> tuple[str, str]:
    match = re.match(r"^(.*?)\s*\[([A-Za-z0-9\-]+)\]\s*$", game_field.strip())
    if match:
        return match.group(1).strip(), match.group(2).strip()
    return game_field.strip(), ""


def parse_table_rows(table_block: str) -> list[list[str]]:
    rows = []
    for line in table_block.splitlines():
        line = line.strip()
        if not line.startswith("|"):
            continue
        if line == PLACEHOLDER_ROW:
            continue
        if re.match(r"^\|\s*-+\s*\|", line) or line.startswith("| Title |"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) == 5:
            rows.append(cells)
    return rows


def render_table(rows: list[list[str]]) -> str:
    header = "| Title | Title ID | Status | Last tested (version) | Reports |\n|---|---|---|---|---|"
    if not rows:
        return header + "\n" + PLACEHOLDER_ROW
    rows_sorted = sorted(rows, key=lambda r: r[0].lower())
    body = "\n".join(f"| {' | '.join(r)} |" for r in rows_sorted)
    return header + "\n" + body


def main() -> int:
    body = os.environ.get("ISSUE_BODY", "")
    issue_number = os.environ.get("ISSUE_NUMBER", "")
    issue_url = os.environ.get("ISSUE_URL", "")
    version_hint = os.environ.get("KYTY_VERSION", "").strip()

    fields = parse_issue_fields(body)
    game_field = fields.get("Game (title + Title ID if known)", "").strip()
    status = fields.get("Status", "").strip()
    version = fields.get("KytyPlus version", "").strip() or version_hint

    if not game_field or not status:
        print("Missing Game or Status field, skipping table update.")
        return 0

    title, title_id = split_title_id(game_field)
    report_link = f"[#{issue_number}]({issue_url})"

    with open(COMPAT_PATH, "r", encoding="utf-8") as f:
        content = f.read()

    # COMPATIBILITY.md was split into two tables: "## Native (PS5)" (five
    # columns, maintained from these issues) and "## shadPS4" (four columns,
    # sourced from the upstream list). The old "## Table" heading is gone, which
    # is why this lookup stopped matching.
    #
    # Anchor on the Native table. Group 1 keeps the heading and its prose, group 2
    # is the header/separator/rows that render_table() rewrites. Group 2 ends at
    # the last row, so the blank line before the row-template comment stays in
    # the untouched tail.
    #
    # Do not add a trailing terminator to this pattern. Group 1's `.*?` is lazy,
    # so a terminator it cannot satisfy with the Native table lets it run all the
    # way down to the shadPS4 table and rewrite that one instead -- which also
    # meant the script only ever succeeded once before the anchor moved.
    # No table content is changed by this.
    table_match = re.search(
        r"(## Native \(PS5\)\n\n.*?\n\n)"
        r"(\| Title \|[^\n]*\n\|[-| ]+\|\n(?:\|[^\n]*\n)*)",
        content,
        flags=re.DOTALL,
    )
    if not table_match:
        print(
            "Could not locate the '## Native (PS5)' table in COMPATIBILITY.md",
            file=sys.stderr,
        )
        return 1

    rows = parse_table_rows(table_match.group(2))

    existing = None
    for row in rows:
        same_id = title_id and row[1].strip().upper() == title_id.upper()
        same_title = row[0].strip().lower() == title.lower()
        if same_id or (not title_id and same_title):
            existing = row
            break

    if existing is not None:
        # Newer report wins for Status/Version; keep prior report links, append this one.
        existing[1] = title_id or existing[1]
        existing[2] = status
        existing[3] = version or existing[3]
        if report_link not in existing[4]:
            existing[4] = f"{existing[4]}, {report_link}" if existing[4] else report_link
    else:
        rows.append([title, title_id, status, version, report_link])

    new_table = render_table(rows)
    # Group 2 consumed the final row's newline, and render_table() emits no
    # trailing newline, so put one back. The blank line that separates the
    # table from the row-template comment is still in the tail and is left
    # exactly as it was found.
    new_table += "\n"
    new_content = (
        content[: table_match.start()]
        + table_match.group(1)
        + new_table
        + content[table_match.end(2) :]
    )

    if new_content != content:
        # newline="\n": the default translates to os.linesep, which on a
        # Windows dev machine rewrites all 1352 line endings and turns a
        # one-row change into a whole-file diff. The file is LF in git.
        with open(COMPAT_PATH, "w", encoding="utf-8", newline="\n") as f:
            f.write(new_content)
        print(f"Updated {COMPAT_PATH} for '{title}' from issue #{issue_number}.")
    else:
        print("No changes needed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
