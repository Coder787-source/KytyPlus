# KytyPlus v1.7

**Download:** `KytyPlus-v1.7-windows-x64.zip`

What's new vs v1.4 / recent main:

Graphics

- HTile MSAA depth path: host depth format promote + depth-only view aspects so `--htile-clear-only` / MSAA depth clears no longer fail on common host formats
- Color RT / poly-offset / Standard64KB helpers tightened from public Prospero/RDNA layout rules (bias helper, fast-clear semantics tests)
- Descriptor soft-nulls: known-illegal encodings (numeric class mismatch, illegal sampled-depth IR, zero-size storage, mip/MSAA view failures, footprint/alignment) now **EXIT** instead of silent null binds; unknown tile/depth encodings stay soft-null until a public dump proves layout
- PM4: direct CX handlers for known AMD/Prospero registers (`CB_SHADER_MASK`, `DB_SHADER_CONTROL`, SPI PS/interp/baryc/Z, stencil BF, blend channels, `PA_SC_SHADER_CONTROL`, multi-prim reset indx, poly-offset burst)
- PM4 soft-ignore dry-run probe (`Pm4SoftIgnore::ProbePacketStream`) for CI / evidence without a live CP

Audio

- NGS2: PCM helpers + VAGp ADPCM decode as headers (`ngs2_pcm.h` / `ngs2_vag_decoder.h`); unsupported waveform types on mix **EXIT** (no invented ATRAC9 / rack ABI)

Loader / #66

- Near-null page soft-skip retained; rolling fingerprint ring (`NullPageFaultFingerprint`) with format/reset APIs for post-mortem matching
- Real #66 root-cause fix still needs guest eboot + faulting RIP

CI / tests

- Focused Windows + Linux subsets: graphics/audio semantics, PM4/NGS2 fuzz (4096 iters + dry-run), soft-ignore evidence, pipeline-cache data, shader CFG; Windows also runs GPU texture-cache slices when an ICD is present
- Public-trace fixture stub + soft-null audit doc (`tests/fixtures/public_traces/`)

Expectations

- This release is **synthetic / public-docs progress** — no new game dumps were used to validate playability
- Boots further / fewer soft-nulls ≠ playable; verify with your own logs before claiming a title works
- Still blocked without dumps: unknown CX/SH/UC meanings, remaining depth/storage tile encodings, ATRAC9/NGS2 effect ABI, live #66 guest fix

Tag `v1.7`, GPL-2.0.
