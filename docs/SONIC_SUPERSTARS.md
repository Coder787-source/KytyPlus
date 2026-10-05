# Sonic Superstars (PPSA06888)

## Compatibility

**Playable**, based on sustained user-reported gameplay on a Ryzen AI 5 340 / Radeon
840M and directly observed Jungle gameplay after the PlayGo fix. Tested game
version: **01.001.008**, using a local source build, not a newly released version.
The user reports working respawns/checkpoints, no missing objects, and occasional
FPS dips that recover within seconds.

This is not a completed-campaign certification, a locked-60-FPS claim, or a
statistical guarantee. Later levels, all bosses, and normal campaign progression
have not been independently verified. Later tests used the optional temporary
unlock with an isolated save. The tested local build also contained graphics work
outside this Sonic-focused change; identical performance on every clean build or
GPU is not established. Historical issue #12 remains linked separately.

The "Downloading data. Please wait." gate was traced to two-pass PlayGo chunk
enumeration. Null-buffer queries now return the total before the buffer-fill
pass. If install metadata is absent, the local-install fallback enumerates IDs
0..999, matching the dumped PlayGo stub, and reports requested chunks as locally
installed. This does **not** supply missing files or download anything. Use a
complete, legally obtained dump.

## Default graphics profile (all native-engine titles)

New CLI launches and fresh launcher settings default to:

- Engine resolution request: **1920 x 1080**; native guest attachments are retained.
- Upscaler: **Fsr1**, Performance preset, sharpening **0.3**.
- FSR result: **3840 x 2160**, independently of the host window size.
- FIFO presentation (existing default).

The existing Fsr1 path is this project's FSR-inspired spatial EASU/RCAS upscaler,
not temporal FSR 2/3 or frame generation. Engines may ignore the screen-width /
screen-height argv request; defaults cannot force every game to render at 1080p.
Runtime stderr reports `[render-source]` and `[fsr] active` with actual dimensions.
Sonic's tested framebuffer was 1920x1080 with a 3840x2160 FSR result. Smaller windows
or displays downsample that result; they do not become physical 4K displays.
No measured speedup is claimed; computing a 4K result adds GPU work.

Existing explicitly saved launcher choices (including Off) are preserved.
Choose Fsr1 in Graphics settings to enable it for an existing configuration.
PS4 games delegated to shadPS4 are unaffected by the native-engine defaults.

CLI overrides:

```text
kyty_emulator --game "D:/Games/Superstars" --upscaler-method Off
kyty_emulator --game "D:/Games/Superstars" --guest-render-width 0 --guest-render-height 0
kyty_emulator --game "D:/Games/Superstars" --fsr-output-width 0 --fsr-output-height 0
```

These respectively disable FSR, disable the engine resolution request, and use
the window extent for FSR output. Each dimension pair must be supplied together;
nonzero dimensions are limited to 240..7680. Failed Vulkan FSR creation/dispatch
is logged and falls back to a plain blit, not a false claim of active FSR.

## Optional temporary level unlock (Windows, Python 3)

Ordinary launches do **not** unlock levels. The helper patches only its own new
emulator process after the dynamic module loads. Closing it removes the memory
patch. Game files on disk are not modified.

From the repository root, first validate your dump:

```text
python scripts/sonic_superstars_unlock.py --game "D:/Games/Superstars" --check
```

Then close any running emulator and start the isolated session:

```text
python scripts/sonic_superstars_unlock.py --game "D:/Games/Superstars"
```

The default emulator and normal saves are expected under `build/src/launcher`.
For another layout pass `--emulator`, `--save-dir` (the PPSA06888 directory), and
optionally `--session-dir`. The save and session directories must be separate.

The helper verifies title/version, the entire module SHA256, executable ranges,
and original instruction guards before writes. It copies saves into a separate
session, verifies the initial backup, and checks normal-save hashes at startup
and exit. The isolated session persists across helper launches; its progress is
never copied back. Do not edit normal saves during testing. Keep an independent
backup of valuable progress. Every stage-selection option has not been tested.

FSR4K is the helper default too; `--graphics-profile native` disables FSR.
J confirms/jumps, K goes back, arrows move, and Enter pauses.

[The helper plan](../patches/sonic-superstars-temporary-unlock.json) is a
version-specific **helper-only** schema for a dynamic module, not the emulator's
`--game-patch` / `_Patches` format. No dumps, saves, logs, or disassembly are shipped.

## Regression checks

Build `playgo_api_tests`, `graphics_audio_semantics_tests`, and
`launcher_configuration_tests`; run their registered CTest cases. The PlayGo test
uses real production guest APIs/resolver with a substituted metadata provider,
covering two-pass enumeration, fallback/manifest bounds, caching/reset, installed
locus/progress/ETA, and commerce imports. Python helper checks run with
`python -m unittest discover -s tests -p 'test_sonic_superstars_unlock.py'`.
