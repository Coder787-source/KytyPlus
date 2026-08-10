# KytyPlus v1.9 — Cross-Platform Performance & Infrastructure Release

Tag: v1.9 · License: GPL-2.0

## Downloads

- **Windows:** `KytyPlus-v1.9-windows-x64.zip`
- **macOS:** `KytyPlus-v1.9-macOS-x86_64.zip`
- **Linux:** `KytyPlus-v1.9-linux-x86_64.zip`

All three builds are produced and verified automatically by CI on every tagged release (see *Infrastructure* below).

---

## What's new vs v1.8

### Performance

- **Translation-block cache keyed by guest PC (`src/core/ICache.hpp`).**
  Translated guest code blocks are cached by their guest address and looked up
  on subsequent execution of the same address, so a block is translated once
  and reused rather than re-translated each time it is reached. Blocks are
  stored in a page-aligned executable memory pool that is allocated once at
  startup, avoiding repeated executable-memory syscalls per block. A `Flush()`
  primitive is provided to invalidate cached blocks when guest code is modified
  or unloaded.

  > Note: This is an address-keyed block cache with a single executable pool.
  > It is **not** an LRU cache, has no fixed entry cap, and no eviction policy.
  > No performance multiplier is claimed for this release — see *Benchmarks*
  > below.

- **60 FPS frame-pacing target (`src/graphics/presentation/window/window.cpp`).**
  The render loop now targets a 16.67 ms per-frame interval. This is a pacing
  *target* in the render loop; it is **not** a measured gameplay framerate and
  does not guarantee 60 FPS in any title. Actual framerate depends on the
  emulated title, the host hardware, and I/O (see *Benchmarks*).

### Launcher

- **Direct `QProcess` launch (`src/launcher/src/mainDialog.cpp`).**
  The launcher now spawns `kyty_emulator` directly via `QProcess` instead of
  wrapping it in `cmd.exe /K`. This removes the stray console window and avoids
  the path-separator issues that previously affected launching from external
  drives.

### Infrastructure

- **Cross-platform CI releases (`.github/workflows/build.yml`).**
  CI now builds and verifies Windows, macOS, and Linux artifacts on every push
  and pull request, and automatically packages and publishes all three zips as
  GitHub release assets when a `v*` tag is pushed. Each platform job verifies its
  own binary (e.g. `ldd`/`readelf` checks on Linux, `file` checks on macOS)
  before the release job uploads anything.

### Bug fixes carried forward from v1.8

- Event-flag `Clear()` bitmask inversion fixed.
- Thread-unsafe singleton fixed.
- ELF loader compile error fixed.
- HD haptics support for DualSense.
- Ray-tracing extension loading.
- Launcher external-drive path fix.
- Various stability fixes (`CloseAll`, `FillRandomBuffer`, `quick_exit`,
  `UniqueFunction`, event queue).

---

## Benchmarks

**No benchmark claims are made for v1.9.**

The only measured result currently available for KytyPlus is from **v1.8**:

- Dead Cells boots to its main menu, rendering cleanly with no graphical
  glitches, holding at menu for ~15 seconds in the recorded footage, at
  approximately **17 fps**.

- Test rig: i7-9700K @ 5.0 GHz, RTX 4060 Ti, 16 GB RAM, Windows 11.
- The game dump was run from an **external HDD** (the tester's normal
  emulation-storage setup) rather than an NVMe, which likely bottlenecked the
  result. This figure is therefore a lower bound on that hardware, not a peak.

v1.9 has **not** been benchmarked on the test hardware yet. Any specific FPS
number for v1.9 would be unverifiable and is intentionally omitted. When v1.9
is tested under comparable conditions and captured on video, this section will
be updated with the measured figure and a link to the footage.

## Expectations

This release is a **performance and infrastructure milestone**, not a
performance *result*. It ships the engineering for cross-platform CI and a
direct-launch launcher, and lays groundwork (address-keyed block cache,
frame-pacing target) for future measurement. It does **not** claim a framerate
improvement, because no v1.9 measurement exists yet.