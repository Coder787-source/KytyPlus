# Public dump traces for this title class

No redistributable PM4 / register / NGS2 audio command dumps were located for
the Prospero title class under test.

Searched:

- KytyPS5 repository fixtures / issue attachments
- Public GitHub references to PS5 PM4 or NGS2 dumps suitable for CI
- shadPS4 Liverpool queue-dump tooling (PS4 Liverpool, not Prospero)

Until a license-clear public dump exists, rely on:

- `GraphicsAudioSemanticsTests` for poly-offset, color fast-clear, Standard64KB,
  NGS2 PCM/VAG, and null-page skip length
- `Pm4Ngs2FuzzTests` for random-but-valid packet / waveform shaping
- Existing Vulkan texture-cache subsets (`--htile-clear-only`, etc.)
