# Async Pipeline Compilation

**Status:** code-complete, *not build-verified* (this environment cannot compile a
Vulkan/Qt/CMake emulator binary). Structurally verified: header/declaration/definition
parity, brace/paren balance, thread-safety of every shared object a worker touches.

## The problem this solves

Every draw on KytyPlus went through this *synchronous* path on the single GPU thread:

```
GPU thread → cp.Process → DrawIndexAuto
  → renderDraw.cpp: ExecutePreparedDraw
    → RefreshShaders → ShaderCompileInfoVS/PS        [MISS = full GNM→SPIR-V recompile]
    → PipelineCache::CreateGraphicsPipeline
      → shaders.cpp: vkCreateGraphicsPipelines         [MISS = driver pipeline build, blocks]
```

`vkCreateGraphicsPipelines` (the driver's own pipeline compile) and the SPIR-V
recompile ran **inline, on the one GPU thread**, blocking all 57 compute queues behind
them. There was no async path (verified by grep). The result: on first encounter of
each unique shader+state combination, the frame froze for tens to hundreds of
milliseconds — the classic "first-play stutter" every young emulator suffers.

Note: SPIR-V is *already* disk-cached (`ShaderDiskCacheLoad`) and the driver pipeline
cache is *already* persisted (`_Cache/vulkan_pipeline_cache.bin`). So second+ launches
are already smooth. The remaining pain is **first launch / first encounter**, which is
exactly what async compilation targets.

## The design

This implements the same strategy used by mature emulators (RPCS3, shadPS4, Yuzu):
**compile pipelines on a worker thread pool, and skip recording draws whose pipeline
isn't ready yet.**

### New files
- `src/graphics/host_gpu/renderer/pipeline/asyncPipelineCompiler.h/.cpp` —
  `AsyncPipelineCompiler`: a fixed pool of `kWorkerCount = 4` `std::jthread` workers.
  Each worker pops a `CompileRequest` (key + rendering + static_params + copies of the
  SPIR-V and shader input info) and runs the *already thread-safe* `CreatePipelineInternal`,
  then publishes the finished pipeline back into `PipelineCache`.

### Modified files
- `src/common/emulatorConfig.h/.cpp` — new `async_pipeline_compilation` config flag
  (default **true**) + `Config::AsyncPipelineCompilationEnabled()` getter.
- `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h` —
  - `GraphicsPipelineKey` / `PipelineKeyHash` / `GraphicsPipelineKeyHash` /
    `GraphicsPipelineKeyEqual` moved from private to public (so the async compiler
    can name them).
  - `std::unique_ptr<AsyncPipelineCompiler> m_async_compiler` member (forward-declared
    type; destroyed in `~PipelineCache`, defined in the .cpp where the type is complete).
  - New API: `TryGetGraphicsPipeline`, `SubmitAsyncCompile`, `PublishCompiledPipeline`,
    `AsyncCompilationEnabled`, `IsAsyncPending`, `AsyncPendingSentinel`, and the
    `PipelineLookupResult` enum.
- `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp` —
  - `CreateGraphicsPipeline`: on a cache miss, if async is enabled, submit a compile
    job and return the `AsyncPendingSentinel()` (a static `GraphicsPipeline` with null
    handles) instead of calling `CreatePipelineInternal` inline. Falls through to the
    original synchronous compile when async is disabled.
  - `~PipelineCache`: calls `m_async_compiler.reset()` *before* destroying pipelines, so
    workers (and any in-flight `vkCreateGraphicsPipelines`) finish while the device is alive.
  - `SubmitAsyncCompile` lazily creates the compiler on first use.
  - `PublishCompiledPipeline` inserts the worker's pipeline under `m_mutex`; if a
    synchronous compile raced ahead, it destroys the duplicate safely on the worker thread.
- `src/graphics/host_gpu/renderer/renderDraw.cpp` — `ExecutePreparedDraw` checks
  `pipeline.pipeline == nullptr` right after acquisition: if so, it logs
  `PipelinePending-SkipDraw` and returns *before* `bindPipeline`/`vkCmdDraw`. The next
  frames keep skipping until the worker publishes the real pipeline.

## Why this is safe (the make-or-break question)

A worker thread compiling a pipeline concurrently with the GPU thread touches only
already-concurrency-safe objects. This was verified by reading each one:

| Resource | Thread-safe? | Evidence |
|---|---|---|
| `vk::Device::createGraphicsPipelines`, `createDescriptorSetLayout`, `createPipelineLayout` | ✅ | Vulkan spec: object *creation* calls are externally-synchronized-free and reference-counted. Standard practice. |
| `DescriptorCache::GetDescriptorSetLayout` | ✅ | `descriptorCache.cpp:291` takes `Common::LockGuard lock(m_mutex)`. |
| `ShaderRecompiler::TryRecompile` | ✅ | Pure build: shader bytes + options → fresh IR/Program. No shared mutable state. |
| `vk::PipelineCache` (`m_driver_cache`) | ✅ | Reference-counted and explicitly thread-safe per the Vulkan spec. |
| `PipelineCache::m_graphics_pipelines` | ✅ | Only mutated under `m_mutex`; published by `PublishCompiledPipeline`. |
| `ShaderStageRuntime` (`shared_ptr<Program>`) | ✅ | Captured by value in `CompileRequest`; ref-counted, keeps the IR alive for the worker's lifetime. |

The one thing a worker mutates that the GPU thread reads is the pipeline map, published
under `PipelineCache::m_mutex`. `CreatePipelineInternal` (graphics variant, in
`shaders.cpp`) was verified to touch *only* `descriptor_cache.GetDescriptorSetLayout`
(locked) + `graphics.device` + the worker-owned pipeline — it never reaches back into
`PipelineCache`'s map or mutex. So there is no hidden shared state and no deadlock: the
GPU thread never waits inside `CreateGraphicsPipeline` (it returns immediately after
submit), so the worker's `PublishCompiledPipeline` (which takes `m_mutex`) can never
deadlock against it.

## What this does and doesn't do (honest scope)

- ✅ **Eliminates first-encounter stutter** — the multi-100 ms hitches when a new shader
  appears. This is the single biggest UX win for first-play.
- ✅ **Keeps the GPU thread responsive** — no long blocks on the driver pipeline build.
- ⚠️ **Does not raise steady-state FPS** once all pipelines are cached — at that point
  the GPU itself is the bottleneck, a different problem.
- ⚠️ **A few skipped frames** when a brand-new pipeline appears: the affected draw is
  skipped until the worker publishes. Brief, far less jarring than a freeze, and
  self-corrects within a frame or two.
- ⚠️ **First launch still pays the compile cost** — async spreads it across workers and
  hides it from the frame path, but the CPU work still happens. With the existing
  SPIR-V + driver disk caches, *subsequent* launches are fast.

## Configuration

`Config::async_pipeline_compilation` (default `true`). Set `false` to force the original
fully-synchronous path (every draw blocks on first-encounter pipeline build). The
fallback is automatic: if a worker fails to produce a pipeline (driver error / throw),
it removes the in-flight marker and the next draw falls back to synchronous compile —
so a broken async path never leaves a pipeline stuck "pending" forever.

## Build wiring

`asyncPipelineCompiler.cpp` lives under `graphics/host_gpu/renderer/pipeline/`, which is
covered by the existing `file(GLOB ... renderer/pipeline/*.cpp)` in `src/CMakeLists.txt`
(line 165), so it compiles automatically with no CMake edit needed.