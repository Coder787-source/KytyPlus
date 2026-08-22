# KytyPlus v2.9

**Tag:** `v2.9` (`e6f4b87`)
**License:** GPL-2.0
**Platforms:** Windows (x64), macOS (Apple Silicon / Rosetta 2 + MoltenVK), Linux

This is a **launcher-and-polish** release. The launcher graduates from "barebones game list" to a full game-library manager with one-click PKG install, per-game controller remapping, and delete. The core emulator received dozens of merge-resolution fixes that unblocked compilation on both macOS and Windows CI. No new game dumps were used to validate — testers are the bottleneck.

## What actually changed vs v2.8

### Launcher — game library (major)

The launcher is now a genuine game manager, not just a list widget:

- **Install Package (.pkg):** Click "Install Package," select a `.pkg` file, and the launcher runs `kyty_emulator --install-pkg` asynchronously in the background. The GUI no longer freezes during extraction. After extraction completes, the launcher recursively copies all files (including nested `sce_sys/` directories) into `games/<content_id>/` and rescans the library. The game appears in the list automatically.
- **Delete game:** Right-click any game in the list and choose "Delete game..." — removes the entire game folder from disk after a confirmation dialog. Disabled while the game is running.
- **Game settings panel:** Click "Go To Settings" on any game to access the full configuration dialog, now with controller mapping at the bottom.

### Controller remapping (new)

The launcher's settings dialog has a new **Controller Mapping** panel with 24 rebindable controls:

- Every PS5 button (Cross, Circle, Triangle, Square, D-Pad, L1–L3, R1–R3, Options, Touchpad, left/right stick axes, triggers) can be rebound to a keyboard key, mouse button, or gamepad button.
- Click any row, press your desired input — the launcher captures whatever you press.
- Mappings are serialized as `--keymap Cross=Key:X --keymap Circle=Pad:B` arguments passed to the emulator at launch.
- Works for wired AND wireless controllers (SDL's `SDL_GameControllerOpen` handles both).
- The emulator-side `InputMap` now understands `Pad:ButtonName`, `Key:Name`, and `Mouse:Button` bindings.
- Gamepad buttons are also routed through the custom map, not bypassed.

### Build fixes (dozens)

The merge from upstream KytyPS5 introduced compile errors across ~20 files. All have been resolved:

- **Memory subsystem:** Missing closing braces in `CommitFixedHostRange` (cascading 20+ errors); added `ReserveAlignedFailed` and `UnmapRollbackFailed` to `FailureReason` enum.
- **Graphics pipeline:** Restored `ImageViewKindCount` → `SampledImageViewKindCount` rename throughout; fixed `depth_bias_constant_factor` / `depth_bias_slope_factor` members; restored `DrawEmitInfo` NGG fields; added missing `Config` include.
- **Shader recompiler:** Restored rect-list shader generation (`BuildRectListShaders`, tessellation module declarations); restored `CanonicalizeNaturalLoops`, `SelectionRegion`, and `block_budget` in CFG; removed duplicate `ImageGradientComponents`; added missing `OpcodeTable.h` include to `VectorAluOps.cpp`; added `using Detail::Lookup` and fixed `FindVop2SdwaRule` arg.
- **Kernel:** Added `PTHREAD_STACK_BOTTOM` constant.
- **Presentation:** Added `hostInput.h` include in `window.cpp`; restored `HostInputKey`, `HostInputMouseButton`, `HostInputInit` paths.
- **Controller:** Restored `ControllerResetInputState` declaration and implementation.
- **PFS parser:** Fixed 8-byte dirent name padding for correct directory enumeration.
- **ImGui:** Added `imgui` target to `kyty_emulator` link libraries (fixes `fatal error: 'imgui.h' file not found`).

### README and docs

- Restored GameGaz press links with v2.7 coverage.
- Accuracy pass — fixed overclaims, updated video/coverage/framing.
- Added explicit legal note for firmware install.
- Reframed issue tracker as a place to ask questions, not just file reports.
- Collaborative tester framing ("we build a better emulator together").

## What was removed

- **Firmware / PUP boot path:** PUP firmware parsing and module loading were reverted. KytyPlus is now purely HLE — no Sony firmware modules are loaded or required. This eliminates legal gray area around firmware distribution and simplifies the boot path.
- **Redundant "Configure Controller" button:** Removed from the main launcher window; controller mapping is now only in the per-game settings dialog (scroll to the bottom).

## CI / build

- Cross-platform matrix (Windows + macOS + Linux) with Vulkan SDK.
- macOS CI now passes past the merge breakage that had it stuck at ~620/773 compiled units.
- Launcher builds against Qt 6.10 with automatic MOC/UIC/rcc and `windeployqt` packaging.

## Known issues at this tag

- End-to-end game validation is still blocked without testers. A Twitter user has volunteered to test once build breakage is resolved.
- iGPU optimization path is wired but unvalidated on real iGPU hardware.
- Shader cache, FSR upscaler, and configurable present path are implemented but game-level benefit is unconfirmed.
- The JIT optimizer (`core/jitOptimizer.*`) remains dead source from v2.1 — not compiled, not called.

## Legal

Clean HLE emulator. Does not ship Sony code or keys, does not decrypt or circumvent DRM, requires already-decrypted content supplied by the user, and does not provide instructions or tooling for producing it. GPL-2.0.

Tag `v2.9`, GPL-2.0.