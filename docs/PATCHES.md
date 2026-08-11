# Game Patches (`_Patches/<TITLE_ID>.json`)

KytyPlus supports **binary patches** for PS5 titles: JSON patch plans that rewrite bytes inside the
loaded guest executable before it runs. This is the mechanism for per-title workarounds (e.g., making a
game tolerate a condition the emulator or a dump can't satisfy) without touching emulator source.

## Where patches live

- The launcher looks for `_Patches/<TITLE_ID>.json` **next to the launcher executable**
  (`QCoreApplication::applicationDirPath()`), e.g. `_Patches/PPSA01603.json`.
- If a plan file exists for the selected game, the launcher passes `--game-patch <path>` to the
  emulator (or you can pass `--game-patch` on the command line directly).
- Only `PPSA*` (PS5) title IDs are supported by the launcher UI. PS4 titles run through the shadPS4
  delegation path and are **not** patched by this system.

## Format

```json
{
  "title_id":     "PPSA01603",
  "game_version": "01.000.013",
  "process":      "eboot.bin",
  "patches": [
    {
      "name":    "Example: NOP the project-file abort",
      "enabled": true,
      "writes": [
        {
          "expected":    "4889C3909090909090",
          "replacement": "909090909090909090"
        }
      ]
    }
  ]
}
```

### Field reference (from `src/loader/gamePatch.cpp`)

| Field | Type | Meaning |
|---|---|---|
| `title_id` | string | Must equal the game's `TITLE_ID` from `sce_sys/param.json` (case-insensitive) |
| `game_version` | string | Must **exactly** equal the game's `APP_VER` (e.g. `01.000.013`) |
| `process` | string | Filename of the main executable; must match (case-insensitive), typically `eboot.bin` |
| `patches[]` | array | List of patches; each with `name`, `enabled` (optional, default true), `writes[]` |
| `writes[].expected` | hex string | The original bytes (even-length hex, no spaces) to search for |
| `writes[].replacement` | hex string | The bytes to write in their place; **must be the same length** as `expected` |

## How it works

1. **Load & validate** — the plan must parse and match the running game (title ID, APP_VER, process
   filename). Any mismatch aborts with `Game patch error: patch plan does not match the loaded game`.
2. **Locate** — each `expected` byte pattern is searched in every loaded `PT_LOAD`/`PT_OS_RELRO` segment
   of the executable, from the segment base up to `p_filesz`.
3. **Apply** — the matched address is made writable (`ExecuteReadWrite`), the `replacement` bytes are
   copied, the original memory protection is restored, and the instruction cache is flushed.

## Rules that keep patches safe

- `expected` and `replacement` must be **identical length**. Patches never grow or shrink code.
- The pattern must be found **at least once**, or the patch fails:
  `Game patch error: original bytes not found for '<name>'`. A stale patch (wrong game update) fails
  loudly rather than corrupting memory.
- **Ambiguity is warned, not hidden**: if a pattern matches more than one site, the emulator prints a
  WARNING and applies the first match. If you see that warning, make `expected` longer until it matches
  exactly one location.

## Writing a real patch (procedure)

1. **Disassemble the game's `eboot.bin`** (e.g. Ghidra with the Prospero/PS5 loader) to the exact
   instruction you want to change — e.g. the branch or call that aborts `GEngineLoop::PreInit`.
2. **Copy the raw machine-code bytes** of that instruction (and a few bytes of padding around it) into
   `expected`.
3. **Design the replacement** of equal length — typically NOPs (`0x90`), or a forced branch to skip the
   abort. Verify the replacement is a valid instruction stream.
4. **Confirm uniqueness** by searching the pattern across the binary first; extend it until it is
   unique.
5. Place the plan at `_Patches/<TITLE_ID>.json` and run from the launcher, or pass `--game-patch`.

## Notes

- Patches are **per game version**: `game_version` must match `APP_VER` exactly, so a game update
  invalidates existing plans (they fail loudly, which is correct — they were written for old bytes).
- Prefer fixing the emulator over patching games. A patch is the right tool when the *game* needs to
  tolerate something (missing dump file, emulator limitation) that real hardware would not present.
- Example use case from this project's triage: Hogwarts Legacy (`PPSA01603`) aborts
  `GEngineLoop::PreInit` after `phoenix.uproject` resolves ENOENT. **Before writing a patch, confirm
  from the full log whether that ENOENT is truly fatal** — console UE4 normally tolerates a missing
  `.uproject`, so the real cause may be elsewhere. If it *is* fatal and the dump cannot provide the
  file, a patch that skips the abort path is the fallback.
