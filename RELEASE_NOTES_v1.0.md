# KytyPlus v1.0

**Download:** `KytyPlus-v1.0-windows-x64.zip`

What’s new vs KytyPS5 upstream:

- Fewer hard EXITs around texture-cache aliases (sampled/storage/RT, mid-mip, page overlaps), layered RTs, storage tile/swizzle/DCC, sampled depth, GPU events, and compressed video-out
- Soft-skips for null `wait_reg_mem` / `write_data` and null-page guest faults; compute SPIR-V failures retry dispatcher then fall back to a no-op stub
- HLE: richer `libSysmodule` load state + soft-success for unknown IDs; NGS2 rack `0x4003`
- iGPU/UMA defaults: persistent pipeline cache, Mailbox→FIFO present, shader validation off, real SPIR-V Performance optimize, VMA budget/block/host-stream tweaks

Still experimental — boots further ≠ playable. Tag `v1.0`, GPL-2.0.
