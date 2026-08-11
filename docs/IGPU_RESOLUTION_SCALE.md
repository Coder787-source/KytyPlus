# Internal Resolution Scale (iGPU bandwidth diet) — design & status

Target hardware: Steam Deck / Radeon 780M class machines (~44–120 GB/s memory bandwidth
vs the PS5's 448 GB/s). Rendering the scene at 50–25% internal resolution and upscaling
with FSR 1.0 is the single biggest bandwidth/fill-rate lever for this class. FSR alone
upscaling a *native-resolution* frame saves nothing — the render targets must be smaller.

## Status (2026-08-09)

- FSR 1.0 (EASU + RCAS) presentation upscaler: **wired and live** (`fsrUpscaler.cpp`,
  `swapchain.cpp:743`), configurable via CLI + launcher GUI.
- Texture LOD bias: **wired and live** (`samplerCache.cpp:133`).
- UMA heap detection: **live** (`vma.cpp`: detects device-local + host-visible +
  host-coherent heaps and sets the `uma_staging_bypass` flag on UMA devices like the
  Radeon 780M). The staging-bypass itself is **detected but not yet wired** into the
  upload path.
- Hardware detection: **live** (`vma.cpp:66`, `deviceType == eIntegratedGpu`), now also
  feeds `Config::ApplyIgpuDefaults()` (FSR 1.0 + LOD bias 1 defaults on iGPUs).
- Internal resolution scale: **config shell — dead code.** `Config::GetResolutionScale()`
  / `GetResolutionScaleFactor()` (`emulatorConfig.cpp:134/146`) have **zero callers**.
  `GraphicContext::resolution_scale` (`graphicContext.h:74`) is declared, never read.
  Render targets are created at guest native size; FSR upscales a native frame.

## Seam map (verified file:line)

| Purpose | Site |
|---|---|
| Config field + accessor | `emulatorConfig.h:102-103`, `emulatorConfig.cpp:134-154` |
| Dead field | `graphicContext.h:74` (`float resolution_scale = 1.0f`) |
| Render-target desc extent (host image size + cache key) | `colorRenderTarget.cpp:274`, `depthRenderTarget.cpp:336` |
| Render-target info extent (framebuffer/scissor basis) | `colorRenderTarget.cpp:255,326` (`view_extent` → `r.extent`), depth `width/height` |
| Render state size (dynamic rendering) | `renderDraw.cpp:501` `AcquireRenderTargets` (`state.width/height` ← `target.extent`), `context.cpp:300` (`renderArea.extent`) |
| Viewport/scissor from guest registers | `renderDraw.cpp:316-334` (`BuildGraphicsDynamicParams`), `354-371` (`SetDynamicParams`) |
| Cache owner index (address-keyed) + aliasing retirement | `textureCache.cpp` `RegisterImage` (~170-219) |
| Present/FSR source | `swapchain.cpp:743-746`, `fsrUpscaler.h:31` (src=guest res, dst=window) |

## Correctness landmines (why this cannot be a few-line change)

1. **Texture upload resampling.** Guest textures are uploaded by copying guest rows into
   the host image. A smaller host image cannot be filled by a plain copy (it would crop);
   it needs a scaling blit (`blitHelper.cpp` exists). Without it, every texture renders
   as a cropped corner.
2. **RT→texture cache aliasing.** The same guest surface is looked up as a render target
   (RT path) and later as a sampled texture (texture path). The cache is address-keyed
   with a compatibility check that compares extent (`textureCache.cpp:~100`). If RT
   descs use scaled extent and texture descs use guest extent, the cache creates two
   images for one surface and the post-processing pass samples a stale native image.
   The scale must be applied **uniformly** to every lookup, or surfaces split.
3. **Texel-coordinate shader access.** Compute/post-processing shaders read/write render
   targets with integer texel coordinates (e.g., `imageLoad`/`imageStore`, `1.0/width`
   math). At scaled res those coordinates no longer map to the host image. Fixing this
   requires scaling texture coordinates in the shader recompiler for render-target
   bindings (the compiler infra exists: `ResourceMaterialization.cpp`, `SrtPatcher.cpp`).

## Implementation plan (in order, each step gated on scale != 1.0 so Native is byte-identical)

1. **Uniform host-image scale at the cache choke point.** Introduce a host-side
   "physical extent" distinct from the guest `info.extent`, applied inside image
   creation so every lookup of a surface resolves to the same scaled image (fixes
   landmine 2 by construction). Verify: a title's menu renders at reduced res with FSR
   upscale; no cache-split log lines.
2. **Scaled uploads.** Route texture uploads through a scaling blit when physical extent
   != guest extent (fixes landmine 1). Verify: textures no longer cropped; UI text
   readable.
3. **Viewport/scissor scaling.** Scale `BuildGraphicsDynamicParams` viewport
   (x/y/width/height) and the final scissor by the factor, clamped to the scaled
   framebuffer extent. Verify: geometry is not offset/cropped at 0.5 scale.
4. **Shader-coordinate scaling for render-target bindings** (fixes landmine 3): scale
   texel-space coordinates in the recompiler for images whose binding resolves to a
   render target. Verify: post-processing passes (bloom/DOF/SSAO) produce correct output
   at 0.5 scale.
5. **Present path**: confirm FSR source extent is the physical (scaled) extent, and the
   blit/scanout uses the image's real size. Verify: fullscreen output fills the window.

Test matrix per step: Dead Cells (menu + ingame, pixel-art — confirms Nearest fallback
still crisp), a UE4 title menu (post-processing heavy), and a compute-heavy title.

## Already-shipped incremental wins (safe, live)

- `Config::ApplyIgpuDefaults()` (emulatorConfig.cpp): iGPU machines default to FSR 1.0 +
  LOD bias 1 without touching the render path.
- Hardware detection + UMA allocator tuning (`vma.cpp`).
