# KytyPlus v2.1

**Tag:** `v2.1` (`b1e98ce`)
**License:** GPL-2.0
**Platforms:** Windows (x64), macOS (Apple Silicon / Rosetta 2 + MoltenVK), Linux

This is a **scaffolding / public-docs progress** release. No game dumps were used to validate any of the new subsystems. "Compiles and links" ≠ "runs a game." Verify against your own legitimately-dumped, already-decrypted content before claiming a title works.

## What actually changed vs v2.0

The substantive, wired-in changes (these are real and reach running code):

### Graphics / GPU
- Ray-tracing pipeline wiring expanded (`graphics/rt/hardware.cpp/.h`): RT function-pointer loading, `const` correctness, and `GetBufferDeviceAddress` moved out of the header into `vma.cpp` so RT builds on all three platforms instead of Windows-only.
- Shader program cache hardened: a PS cache hit is now rejected when the VS descriptor-set presence differs, fixing `descriptor_set` / `lane_mask_mode` key collisions.
- Command processor: added `m_index_offset` so `GE_INDX_OFFSET` carries through to indexed draws.
- Presentation/window: portable-mode and fullscreen-flag plumbing in `window.cpp`.

### CPU / JIT (incomplete)
- A new JIT optimization framework source file was added (`src/core/jitOptimizer.cpp` / `.h`): block caching, block linking, hot/cold thresholds, optimization levels, and stats.
- **Honest caveat:** in this tag the file is **not wired into the build.** It is not in any `file(GLOB ...)` in `src/CMakeLists.txt`, and no code in the tree calls `GetJITOptimizer()` / `JITOptimizer::Initialize()` (the bootstrap that referenced it was reverted before this tag). So at `v2.1` the JIT optimizer is **dead source** — present in the repo, not compiled, not called. It will not affect runtime behavior until a follow-up adds `core/*.cpp` to the glob and wires up a caller.

### Memory
- Virtual-memory subsystem (`common/mmuVirtualMemory`) and an open-world memory manager (`common/openWorldMemory`) were added as scaffolding for paged / large-address-space mapping. Not yet driven by a real guest workload.

### Loader / kernel
- File-system and runtime-linker tweaks for subsystem registration; minor `libKernel` / `libSaveData` adjustments.
- Windows filename sanitization retained from v2.0 (Arcade Spirits save fix).

## New HLE subsystem files (present, scaffolding-grade, NOT validated)

A large set of new HLE source files were added under `src/libs/`. They compile into the target (they're matched by the `libs/*` glob) but **none have been validated against a real eboot**. They are public-docs-derived scaffolding, not working game support:

- **UE4 stack:** `ue4HLE`, `ue4Animation`, `ue4ClassesExtended`, `ue4MissionNatives` — a UObject/UClass/UFunction/UProperty model with class registration and property access, targeted at UE4-era titles.
- **GTA III Definitive Edition:** `gta3Mission` (mission system), `gta3SaveData` (save data), `saveDataChecksum`.
- **RAGE engine:** `rageScripting` (script VM scaffolding), `physicsCollision`, `textureStreaming`.
- **Audio:** `tempest3D`, `tempestAudioFix`.
- **Video:** `videoDecoder` (FFmpeg decode path).
- **Other:** `networkStubs` (network syscall stubs), `syscallExtended`.

Treat all of the above as **unimplemented-but-present** until you confirm otherwise with real logs. They are the framework the next release will fill in, not finished HLE.

## CI / build
- Cross-platform matrix (Windows + macOS + Linux) with a cross-platform Vulkan loader and CMake fixes; Swiftshader / Vulkan SDK fallbacks for headless CI.
- Shader-cfg test subset adjusted for CI stability.

## Known issues at this tag
- `core/jitOptimizer.*` is dead source (see CPU/JIT above). It will not compile or link into `kyty_emulator` and has no callers.
- End-to-end validation is still blocked without dumps: unknown CX/SH/UC register meanings, remaining depth/storage tile encodings, ATRAC9/NGS2 effect ABI, and the live #66 guest fix.

## Legal
Clean HLE emulator. Does not ship Sony code or keys, does not decrypt or circumvent DRM, requires already-decrypted content supplied by the user, and does not provide instructions or tooling for producing it. GPL-2.0.

Tag `v2.1`, GPL-2.0.
