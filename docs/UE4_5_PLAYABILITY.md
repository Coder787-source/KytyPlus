# UE4 / UE5 Playability Roadmap

Goal: make UE4/5 titles on KytyPlus reach **playable** — starting with Hogwarts Legacy, generalizing to
the whole UE4/5 category. "Real behavior preferred" means: implement the actual semantics (correct
ENOENT, correct positional reads, correct syscall behavior), not stubs that return OK.

This document is the working plan. It is **not** a claim that anything below is playable today.
The confirmed ceiling for any title in this project is **"Reaches menu"** (Dead Cells, v1.8).

---

## 1. The rung ladder (status definitions from COMPATIBILITY.md)

| Rung | Meaning | Log signature that proves it |
|---|---|---|
| 0. Dump intact | All files the game needs are present | No `file not found` for files the game actually needs |
| 1. PreInit passes | UE4 engine init completes | No `GEngineLoop.PreInit Failed!`; game continues past it |
| 2. First frame | A frame is presented | No EXIT guard hit; presentation/video-out active |
| 3. Menu | Title screen renders | Splash/menu visible; input responds |
| 4. Ingame (broken) | Gameplay starts, wrong | Gameplay reachable; artifacts/crashes documented |
| 5. Playable | Saves, audio, input, sustained fps | Full session, checkpoint round-trip, no crash |

Nothing in this project is past rung 3 today.

---

## 2. Why UE4/5 is the right category to attack

- The engine runs as **ordinary guest code** (x86-64) — no RAGE-style engine-HLE wall, no script-VM
  reimplementation. Physics, Blueprint VM, AI, gameplay all execute natively; correctness depends on the
  CPU translator + syscall HLE, both of which are working (native-speed execution, fault-based emulation
  for divergent instructions, null-page skip for UE4/Unity near-zero-pointer faults).
- The **shader set is shared across every UE4/5 title** (a finite, predictable pass set), so recompiler
  coverage progress generalizes.
- UE4 content ships in **.pak files read at offsets via pread** — the file layer already serves this
  correctly (`KernelPread`: seek → read → seek back, mutex-guarded, 64-bit offsets, verified).
- Confirmed boots in the lineage: SILENT HILL: The Short Message (UE5), Minecraft Legends (UE4),
  Disgaea 6 — boot-capability screenshots only, not playability.

---

## 3. Verified implemented (real behavior, not stubs)

| Subsystem | Evidence |
|---|---|
| File I/O: open/read/pread/lseek, 64-bit offsets | `kernel/fileSystem.cpp` (`KernelPread` verified) |
| Case-insensitive path resolution on Linux/macOS | `MountPoints::GetRealFilename` → `ResolvePathIgnoringCase` |
| Batch memory map (`sceKernelBatchMap2`) | `kernel/memory.cpp` — `[Ok]` in real logs |
| Null-page skip for UE4/Unity | `loader/x64InstructionEmulator.cpp` ("cover the first 64 KiB") |
| APR path resolve (truthful ENOENT) | `libs/libAmpr.cpp:357` |
| Audio: AudioOut / Audio2 / Audio3d / ngs2 / speaker arrays | `audio.cpp`, `audio3d_impl.inc` (real bed/object mix) |
| Input: full DualSense (haptics, adaptive triggers, speaker, mic, LEDs) | `controller.h`, `libPad.cpp` |
| LLE: firmware sysmodules from `.pup` | `firmware/*`, `RuntimeLinker::LoadFirmwareModules` |
| GPU: PM4 dispatch, tiler, caches, RDNA2→IR→SPIR-V recompiler | `graphics/*` — extensive, with known gaps below |

## 4. Known EXIT walls (the things that stop rung 2)

These are the live `EXIT` / `EXIT_NOT_IMPLEMENTED` paths any UE4/5 title can hit. Each is a concrete
"next fix" candidate when a log shows it.

| Wall | Location | Notes |
|---|---|---|
| Unknown PM4 opcode → hard EXIT | `graphicsRun.cpp:841` (`EXIT("unknown op")`) | Dispatch table + soft-ignore list is a curated allowlist, not total coverage |
| GS/ES/NGG shader config EXIT guards | `graphics/shader/shader.cpp` `vs_check()` | Non-default `rsrc1`/`rsrc2` fields (scratch, LDS, offchip LDS, VGPR counts, float mode...) EXIT; NGG is special-cased, not general |
| Reads > 4 GiB in one call EXIT | `fileSystem.cpp` `KernelRead`/`KernelPread` | UE4 reads in chunks; low risk, but a hard wall if ever hit |
| Partial ASTC decode (magenta fallback) | `ImageDecoder.cpp` | `kErrorColor {0xFF,0x00,0xFF,0x80}` for undecodable encodings; mostly irrelevant to UE4 (BCn) |

---

## 5. Milestones

### M0 — Dump completeness (blocker for Hogwarts right now)
Check every file the game resolves. For Hogwarts Legacy (EP1018-PPSA01603):
- `H:/homebrew/PPSA01603-app0/phoenix/phoenix.uproject` — observed ENOENT. If genuinely absent from the
  dump, the reporter must re-dump. If present but ENOENT, that is an emulator path-resolution bug and is
  **the first code fix** (fileSystem/GetRealFilename audit).
- `app0/engine/globalshadercache-sf_ps5.bin` — observed missing; likely non-fatal (UE4 rebuilds), but
  confirm it doesn't gate PreInit.
- **Gating artifact: the full `_kyty.txt` from KytyPS5 issue 209.** The `phoenix.uproject` ENOENT is
  likely a **non-fatal probe** (console UE4 probes the project file and proceeds) — the *real* PreInit
  abort reason is in the log lines between the message block and `GEngineLoop.PreInit Failed!`.
  No code fix for Hogwarts should be written before that log is read.

### M1 — PreInit passes (Hogwarts case study)
With M0's log in hand, fix the first real failure:
- If it is a path/ENOENT bug → fix `fileSystem`/mount resolution (benefits every title).
- If it is an RHI/module/config failure → fix that specific HLE with real semantics.
- If it is a genuinely missing dump file the game cannot proceed without → the answer is the dump, or a
  `_Patches/PPSA01603.json` game patch (binary patch, external to the repo) — last resort, not "real
  behavior."

### M2 — First frame renders
Clear the first EXIT guard a UE4 title hits (section 4). Expected first candidates for UE4:
- A GS/NGG or shader-config guard in `vs_check()`.
- An unknown PM4 opcode (add a real handler, not another soft-ignore, unless the packet is genuinely
  no-op on a serialized CP — soft-ignore is acceptable only where the single-threaded model makes it
  semantically equivalent, e.g. `CpOpRewind`).

### M3 — Menu
Framerate-tolerant; requires video out + input + the first UI shaders. This is where the free tests
(Fortnite UE5, Genshin Unity) validate the whole category cheaply.

### M4 — Ingame (broken)
Gameplay reachable; document artifacts. Typical UE4 issues: texture format edge cases, post-chain
passes, Wwise audio quirks, save-data per-title quirks (cf. the Demon's Souls save workaround in
`libSaveData.cpp`).

### M5 — Playable
- Saves round-trip (checkpoints persist across a relaunch).
- Audio stable (no crackle/crash over a session).
- Input correct (DualSense features where the title uses them).
- Sustained framerate (the last rung; GPU-side performance, not CPU — CPU runs natively).

---

## 6. Free testing protocol (no dump needed)

| Title | Engine | Why |
|---|---|---|
| Fortnite | UE5 | Free; validates the whole UE5 category; exercises modern UE5 pass set |
| Genshin Impact | Unity | Free; validates the Unity guest-code path and null-page handling |

Protocol: build current main → run with LLE on → full log → record the rung reached and the first
EXIT guard or artifact → file as a compatibility report. Each result converts a section-4 wall from
"guess" to "known."

---

## 7. Per-title case notes

- **Hogwarts Legacy (UE4, PPSA01603)**: gated on M0 (uproject ENOENT + full log). UE4 title → no
  engine-HLE wall once PreInit passes.
- **Sackboy (UE4)**: no observed log yet; UE4 title, plausible to menu/ingame; wildcard is shader
  coverage + the Astro-style SDK breadth (it's a Sony-published title).
- **Astro's Playroom / Astro Bot (custom engine)**: best DualSense/audio3d fit; `AstroCompatLayer`
  (repo root) is **scaffolding only — not wired into the HLE path**; the compressed asset
  streaming/IODecompressor path it scaffolds is the likely wall past the menu if it's ever needed.
- **Demon's Souls (Bluepoint)**: deepest first-party progress evidence — `libSaveData.cpp` carries a
  title-specific workaround (PPSA01341/01342); push past that point next.

---

## 8. Working principles

1. **Log first, code second.** No speculative fixes for a failure whose log we haven't read. Every
   milestone above has a log signature.
2. **Real behavior over stubs.** Correct ENOENT, correct pread, correct syscall semantics. A soft-ignore
   is only acceptable where the serialized-CP model makes it semantically equivalent.
3. **Generalize per category, not per title.** Fixes live in the shared surface (file system, shader
   recompiler, PM4 dispatch, syscall HLE) so every UE4/5 title benefits.
4. **Track the ladder.** One row per title in COMPATIBILITY.md; a title is never "playable" without a
   user-submitted report at rung 5.
