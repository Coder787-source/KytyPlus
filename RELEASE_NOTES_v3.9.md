# KytyPlus v3.9

## Highlights

- **Sonic Superstars (PPSA06888) is now Playable** — the game previously hung at
  "Downloading data. Please wait." in Jungle Act 1. Fixed by correcting PlayGo
  `PlayGoGetChunkId` two-pass enumeration (count query then fill) and adding a
  compatibility fallback that enumerates 1000 chunks when install metadata is
  absent. Sustained Jungle gameplay was observed; full-campaign completion and
  all bosses are **not** yet verified.

- **Default upscaling: 1080p native -> 4K FSR** — fresh installs and new CLI
  launches now default to a 1920x1080 engine request upscaled to 3840x2160 via
  Fsr1 (Performance preset, sharpness 0.3, FIFO). No manual setup required.
  This is the project's FSR-inspired spatial EASU/RCAS upscaler, not temporal
  FSR 2/3 or frame generation. Engines may ignore the resolution request;
  actual source/output dimensions are logged at runtime.

  Opt out at any time:
  ```
  kyty_emulator --game "..." --upscaler-method Off
  kyty_emulator --game "..." --fsr-output-width 0 --fsr-output-height 0
  ```
  Previously saved launcher choices (including Off) are preserved.

## Changes

### Compatibility
- Sonic Superstars PPSA06888: Ingame (playable-ish) -> **Playable** ([docs](docs/SONIC_SUPERSTARS.md))
- PlayGo: fix `PlayGoGetChunkId` returning 0 on the count-query pass
- Add `libScePlayGoDialog` and `libSceNpCommerce` HLE stubs for import resolution
- Compatibility table refresh for multiple titles (from #12, #13)

### Tests
- `PlayGoApiTests` — regression coverage for two-pass enumeration, fallback
  bounds, caching, and locus/progress/ETA via the real `SymbolDatabase`
- `LauncherConfigurationTests` — FSR default + user-override persistence

## Notes & limitations
- "Playable" reflects sustained early-game confirmation, not a full playthrough.
- The PlayGo chunk fallback assumes a complete local dump; it does not download
  or synthesize missing files.

---

**Full changelog**: [`v3.8...main`](https://github.com/Coder787-source/KytyPlus/compare/v3.8...main)
