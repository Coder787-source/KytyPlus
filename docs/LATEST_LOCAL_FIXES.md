# Latest package, launcher and graphics changes

This development snapshot combines local changes with the current upstream main branch. It is not a new compatibility rating or a promise of gameplay performance.

## Included

- Data-first NAPS package extraction and inner filesystem handling.
- Read-only `.ffpfsc` image mounting, with an isolated .NET 9 helper.
- Launcher support for double-clicking image entries and opening an image supplied as a command-line path or `--open-image <path>`.
- Confirmed Recycle Bin / Trash deletion of game images and game directories, with protected-directory and running-game checks.
- Per-frame FSR resources, sharpening clamped to its supported range, and clearer guest-resolution request/output-size handling.
- Shader, render-target, kernel and PlayGo compatibility work and regression tests.
- Bounded caching of deterministic compute compilation failures. Code and static stage layout are checked before reuse; runtime-dependent failures are not cached.
- Opt-in presentation, shader and GPU-dispatch timing. These diagnostics are disabled by default.

## Important limits

A guest resolution request may be ignored by a game. Changing the window size or upscaling a finished frame does not reduce native game rendering work. The shader-failure cache avoids repeating failed compilation; it does not implement an unsupported shader.

No sustained ASTRO BOT menu/gameplay FPS target has been established. Experimental interrupt batching, unfinished partial-wave changes, game data, save data, captures, local settings, and generated binaries are not part of this snapshot.

## Package helper and licensing

The native emulator uses a separate helper process for the compressed-image/package decoder. Build the `naps_extractor` target with the .NET 9 SDK available; the helper requires the .NET 9 runtime when published framework-dependent.

The helper's decoder licensing and upstream attribution are included in `src/package/naps/Kraken-LICENSE.txt`, `Kraken-NOTICE.txt`, and `Orbis-MIT.txt`. Keep these files with helper distributions.

## Focused regression commands

```text
ninja -C build launcher shader_failure_cache_tests launcher_configuration_tests graphics_audio_semantics_tests playgo_api_tests
build/src/shader_failure_cache_tests.exe
build/src/launcher/launcher_configuration_tests.exe
build/src/graphics_audio_semantics_tests.exe
build/src/playgo_api_tests.exe
dotnet run --project tests/naps/NapsRegressionTests.csproj --configuration Release
```

## Verification status for this snapshot

- Merged native emulator and launcher builds succeeded; launcher startup was checked successfully.
- NAPS/package/image regression runner passed.
- All 15 shader failure-cache checks, launcher configuration regressions, and PlayGo API regressions passed.
- The graphics/audio semantics test currently does not compile: its pre-existing `TestNullPageSkip` references `Loader::X64InstructionEmulator::EstimateNullPageSkipLength`, which is absent from the current header. This snapshot does not claim that suite passes.
- Launcher staging was rechecked after the user closed the game. SHA-256 comparison confirmed the launcher uses the rebuilt emulator binary; no running game was forcibly closed.

These checks do not replace full title compatibility testing.
