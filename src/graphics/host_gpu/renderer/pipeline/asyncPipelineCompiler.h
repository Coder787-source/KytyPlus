#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_ASYNCPYELINECOMPILER_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_ASYNCPYELINECOMPILER_H_

#include "common/common.h"
#include "common/threads.h"
#include "graphics/host_gpu/renderer/pipeline/pipelineCache.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_set>
#include <vector>

namespace Libs::Graphics {

// AsyncPipelineCompiler
//
// Compiles first-encounter graphics pipelines on a fixed worker thread pool
// instead of inline on the GPU thread. Eliminates the multi-100 ms first-enc
// stutter: the GPU thread submits a compile job and immediately proceeds to
// the next draw (which skips recording while the pipeline is not ready),
// while a worker performs the (already thread-safe) vkCreateGraphicsPipelines
// call in parallel. Once a worker publishes the finished pipeline, subsequent
// draws bind it normally.
//
// Safety: every shared object a worker touches is already concurrency-safe:
//   - vk::Device object-creation calls are externally-synchronized-free per
//     the Vulkan spec and reference-counted.
//   - DescriptorCache::GetDescriptorSetLayout takes its own m_mutex.
//   - ShaderRecompiler::TryRecompile is a pure build with no shared state.
//   - vk::PipelineCache is reference-counted and thread-safe per the spec.
// The only state a worker mutates that the GPU thread reads is the finished
// pipeline map, which is published under PipelineCache::m_mutex.
//
// The compiler owns no Vulkan handles itself: finished GraphicsPipeline
// objects are moved into PipelineCache::m_graphics_pipelines (which already
// owns and destroys them), so lifetime is handled by the existing cache.
class PipelineCache;

class AsyncPipelineCompiler {
public:
	// A compile request. SPIR-V and input info are captured by value so the
	// request is fully self-contained and safe to process on a worker thread
	// without referencing GPU-thread-owned state.
	struct CompileRequest {
		PipelineCache::GraphicsPipelineKey key {};

		PipelineRenderingState   rendering     {};
		PipelineStaticParameters static_params {};
		ShaderVertexInputInfo    vs_input_info {};
		// ps_input_info is optional (ps_active == false -> no pixel shader).
		std::unique_ptr<ShaderPixelInputInfo> ps_input_info_storage;
		std::vector<uint32_t>                vs_spirv {};
		std::vector<uint32_t>                ps_spirv {};

		uint32_t vs_hash0 = 0;
		uint32_t vs_crc32 = 0;
		uint32_t ps_hash0 = 0;
		uint32_t ps_crc32 = 0;
		bool     ps_active = false;
	};

	AsyncPipelineCompiler(GraphicContext& graphics, DescriptorCache& descriptor_cache,
	                      vk::PipelineCache driver_cache, PipelineCache& owner);
	~AsyncPipelineCompiler();

	KYTY_CLASS_NO_COPY(AsyncPipelineCompiler);
	// Not movable: held by PipelineCache via std::unique_ptr; the worker threads
	// capture references to the object's members, so it must have a stable address.

	// Submit a compile job for a key that is known to be absent from the cache.
	// If a job for this key is already queued/in-flight, this is a no-op.
	void Submit(CompileRequest request);

	// Returns true if a compile job for the given key is queued or in-flight
	// (i.e. the pipeline is not yet ready). The GPU thread uses this to decide
	// whether to skip recording the current draw.
	[[nodiscard]] bool IsPending(const PipelineCache::GraphicsPipelineKey& key);

	// Shut down the pool, draining queued jobs so the workers don't outlive the
	// Vulkan device. Called from PipelineCache destruction on the main thread.
	void Shutdown();

private:
	void WorkerLoop();

	GraphicContext&  m_graphics;
	DescriptorCache&  m_descriptor_cache;
	vk::PipelineCache m_driver_cache;
	PipelineCache&    m_owner;

	// Worker coordination. m_state_mutex guards the queue, in-flight set, and
	// the stop flag; workers wait on m_work_signal while idle.
	std::mutex                       m_state_mutex {};
	std::condition_variable          m_work_signal {};
	std::vector<std::jthread>        m_workers {};
	std::vector<CompileRequest>      m_queue {};
	std::unordered_set<PipelineCache::GraphicsPipelineKey,
	                    PipelineCache::GraphicsPipelineKeyHash,
	                    PipelineCache::GraphicsPipelineKeyEqual>
	    m_in_flight {};
	std::atomic<bool>                m_stopping {false};

	// Fixed pool size. More than ~4 contends on the driver's internal pipeline
	// compile locks without improving throughput.
	static constexpr std::size_t kWorkerCount = 4;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_ASYNCPYELINECOMPILER_H_
