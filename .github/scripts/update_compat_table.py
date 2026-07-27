#!/usr/bin/env python3
"""Update docs/COMPATIBILITY.md from a closed [Compatibility report] issue.

Triggered only for issues closed as "completed" with the `compatibility` label
(see .github/workflows/compat-table-update.yml) — a maintainer closing the
issue is the review step; this script just saves the manual markdown editing.
"""
import os
import re
import sys

COMPAT_PATH = "docs/COMPATIBILITY.md"

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

    table_match = re.search(
        r"(## Table\n\n)(.*?)(\n\n<!--)", content, flags=re.DOTALL
    )
    if not table_match:
        print("Could not locate ## Table block in COMPATIBILITY.md", file=sys.stderr)
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
    new_content = (
        content[: table_match.start()]
        + table_match.group(1)
        + new_table
        + table_match.group(3)
        + content[table_match.end() :]
    )

    if new_content != content:
        with open(COMPAT_PATH, "w", encoding="utf-8") as f:
            f.write(new_content)
        print(f"Updated {COMPAT_PATH} for '{title}' from issue #{issue_number}.")
    else:
        print("No changes needed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
