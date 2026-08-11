# Compatibility List

Community-reported results for KytyPlus. Entries here come **only** from user-submitted
[Compatibility report](https://github.com/Coder787-source/KytyPlus/issues/new?template=compatibility.yml)
issues — nobody on this project claims to have personally verified every row.

> [!CAUTION]
> "Boots further" is not "playable." This table tracks how far a title gets, not whether it's
> recommended to play. See the [README](../README.md) for what KytyPlus is / is not.

## Status definitions

These match the dropdown in the compatibility report template, so a submitted issue maps
directly onto a row here.

| Status | Meaning |
|---|---|
| Does not boot | Crashes or hangs before any video output |
| Boots / logos only | Splash/logo screens render, nothing playable reached |
| Reaches menu | Title screen or main menu is reachable |
| Ingame (broken) | Gameplay starts but is broken (crashes, corrupt visuals, stuck) |
| Ingame (playable-ish) | Gameplay is reachable and roughly functions, not a quality guarantee |
| Playable | Gameplay is reachable and holds up well enough to actually play through |
| Other | Doesn't fit the above — see notes column |

## How this list is maintained

1. Testers file a [Compatibility report](https://github.com/Coder787-source/KytyPlus/issues/new?template=compatibility.yml)
   issue for a **legally obtained** title.
2. A maintainer reviews the report (sanity-checks the log/version, no dump/firmware requests) and
   closes the issue as **completed**. Closing as completed is the approval step — a bot
   (`.github/workflows/compat-table-update.yml`)
   then parses the issue form and commits the row below automatically, so the table can't drift
   out of sync with approved issues. Close as **not planned** instead if a report should not be
   added (spam, dump/firmware request, insufficient info).
3. If a newer report contradicts an older row for the same title, the newer report wins — the bot
   updates Status/Version in place and appends the new issue link to the Reports column so history
   isn't lost.
4. Rows are sorted alphabetically by title automatically. Keep one row per title; the bot matches
   existing rows by Title ID first, falling back to title text.

Don't have games to test yourself? Filing accurate reports for titles you already own is the
single most useful contribution you can make here — see
[CONTRIBUTING.md](../CONTRIBUTING.md) for details.

## Table

| Title | Title ID | Status | Last tested (version) | Reports |
|---|---|---|---|---|
| Dead Cells | PPSA-15554 | Reaches menu | v1.8 | [#3](https://github.com/Coder787-source/KytyPlus/issues/3) · [footage](https://drive.google.com/file/d/1_7IoA9B2iV-H1VUGtYyxEGiN6PYI2vbu/view?pli=1) (past-menu not tested) |

<!--
Row template (copy/paste and fill in from the issue). Use a full issue URL, not a relative link:
| Game Name | PPSAxxxxx | Ingame (broken) | v1.3 | [#123](https://github.com/Coder787-source/KytyPlus/issues/123) |
-->
