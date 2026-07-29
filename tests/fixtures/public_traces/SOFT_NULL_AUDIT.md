# Soft-null / soft-ignore evidence audit

## Descriptor soft-nulls (descriptors.cpp)

| Path | Status |
|------|--------|
| addr==0 / size==0 unbound storage | Keep null bind (valid unbound) |
| storage footprint overflow | EXIT |
| storage range / alignment / offset adjust | EXIT (host cannot represent) |
| writable/mapped address range failures | EXIT |
| invalid mip / MSAA view encoding | EXIT |
| sampled numeric class mismatch | EXIT |
| storage numeric class mismatch | EXIT |
| storage size==0 (non-null descriptor) | EXIT |
| depth view/format class mismatch | EXIT |
| illegal sampled-depth IR resource shape | EXIT |
| unsupported storage tile/encoding | soft-null until dump proves layout |
| unsupported sampled depth encoding | soft-null until dump proves layout |

## NGS2 beyond PCM/VAG

- PCM + VAGp decode: implemented
- Unsupported `waveform_type` on mix: EXIT (no silent drop)
- ATRAC9: codec docs exist (LibAtrac9 / VGAudio) but **NGS2 waveform type IDs and rack mix ABI are not public** → blocked
- Reverb / submixer / mastering effects: rack create accepted; voice controls EXIT_NOT_IMPLEMENTED; no invented DSP

## PM4 CX/SH/UC soft-ignores

- Dry-run probe: `Pm4SoftIgnore::ProbePacketStream` + `SoftIgnoreEvidenceTests` + fuzz dry-run
- Hardened from public RDNA/clearstate (direct + indirect):
  - `PA_SU_POLY_OFFSET_*`
  - `CB_SHADER_MASK`, `CB_BLEND_{RED,GREEN,BLUE,ALPHA}`
  - `DB_SHADER_CONTROL`, `DB_STENCILREFMASK` / `_BF`
  - `SPI_PS_INPUT_{ENA,ADDR}`, `SPI_INTERP_CONTROL_0`, `SPI_PS_IN_CONTROL`, `SPI_BARYC_CNTL`, `SPI_SHADER_Z_FORMAT`
  - `PA_SC_SHADER_CONTROL`, `VGT_MULTI_PRIM_IB_RESET_INDX`
- Remaining uncovered offsets stay soft-ignore until a public Prospero dump names them

## #66 null-page AV

- Stopgap: `TrySkipNullPageAccess` + rolling `NullPageFaultFingerprint` ring (`ResetNullPageFaultLog`, `FormatNullPageFaultFingerprint`)
- Real fix blocked without guest eboot + faulting RIP / module mapping
