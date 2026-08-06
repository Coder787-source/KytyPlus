#include "graphics/host_gpu/renderer/pipeline/asyncPipelineCompiler.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "graphics/host_gpu/renderer/debug.h"
#include "graphics/host_gpu/renderer/pipeline/pipelineCache.h"

#include <algorithm>
#include <utility>

namespace Libs::Graphics {

AsyncPipelineCompiler::AsyncPipelineCompiler(GraphicContext& graphics,
                                             DescriptorCache& descriptor_cache,
                                             vk::PipelineCache driver_cache,
                                             PipelineCache& owner)
    : m_graphics(graphics),
      m_descriptor_cache(descriptor_cache),
      m_driver_cache(driver_cache),
      m_owner(owner) {
	// Workers are created up front and idle until jobs arrive. Using std::jthread
	// means the threads are joined automatically on destruction, but we still
	// implement explicit Shutdown() so the Vulkan device is guaranteed alive for
	// any in-flight createGraphicsPipelines calls before PipelineCache's members
	// (and the device) are destroyed.
	for (std::size_t i = 0; i < kWorkerCount; i++) {
		m_workers.emplace_back([this] { WorkerLoop(); });
	}
	LOGF("AsyncPipelineCompiler: started %zu worker threads\n", kWorkerCount);
}

AsyncPipelineCompiler::~AsyncPipelineCompiler() {
	Shutdown();
}

void AsyncPipelineCompiler::Shutdown() {
	{
		std::scoped_lock lock(m_state_mutex);
		if (m_stopping) {
			return;
		}
		m_stopping = true;
	}
	m_work_signal.notify_all();
	for (auto& worker: m_workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	m_workers.clear();
}

void AsyncPipelineCompiler::Submit(CompileRequest request) {
	{
		std::scoped_lock lock(m_state_mutex);
		// Per-key dedup: never queue the same pipeline twice. If a job is already
		// queued or in flight, the GPU thread's draw-skip path will keep retrying
		// until the worker publishes it, so a duplicate submit is pointless.
		if (m_in_flight.find(request.key) != m_in_flight.end()) {
			return;
		}
		m_in_flight.emplace(request.key);
		m_queue.push_back(std::move(request));
	}
	m_work_signal.notify_one();
}

bool AsyncPipelineCompiler::IsPending(const PipelineCache::GraphicsPipelineKey& key) {
	std::scoped_lock lock(m_state_mutex);
	return m_in_flight.find(key) != m_in_flight.end();
}

void AsyncPipelineCompiler::WorkerLoop() {
	for (;;) {
		CompileRequest req {};
		{
			std::unique_lock lock(m_state_mutex);
			m_work_signal.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
			if (m_stopping && m_queue.empty()) {
				return;
			}
			if (m_queue.empty()) {
				// Spurious wakeup during shutdown: stop without work.
				if (m_stopping) {
					return;
				}
				continue;
			}
			req = std::move(m_queue.front());
			m_queue.erase(m_queue.begin());
			// Note: keep the key in m_in_flight until the pipeline is published so
			// concurrent IsPending() checks keep returning true (and the GPU thread
			// keeps skipping) until the finished pipeline is visible in the cache.
		}

		// Build the pipeline on this worker thread. CreatePipelineInternal is
		// concurrency-safe (see header rationale): it only touches
		//   - graphics.device object-creation calls (thread-safe per Vulkan spec),
		//   - DescriptorCache (locks its own m_mutex),
		//   - the driver_cache vk::PipelineCache (reference-counted, thread-safe),
		//   - the fresh GraphicsPipeline under construction (worker-owned until
		//     published).
		auto cached = std::make_unique<PipelineCache::GraphicsPipeline>();
		cached->vs_shader_id = req.key.vs_shader_id;
		cached->ps_shader_id = req.key.ps_shader_id;

		const ShaderPixelInputInfo* ps_input_ptr =
		    req.ps_active && req.ps_input_info_storage ? req.ps_input_info_storage.get()
		                                                : nullptr;

		LogPipelineTrace("AsyncWorker CreatePipelineInternal begin", req.vs_hash0,
		                 req.vs_crc32, req.ps_hash0, req.ps_crc32);
		CreatePipelineInternal(m_graphics, m_descriptor_cache, *cached, m_driver_cache,
		                        req.rendering, req.vs_input_info, req.vs_spirv, ps_input_ptr,
		                        req.ps_spirv, req.static_params, req.vs_hash0, req.vs_crc32,
		                        req.ps_hash0, req.ps_crc32, req.ps_active);
		LogPipelineTrace("AsyncWorker CreatePipelineInternal done", req.vs_hash0,
		                 req.vs_crc32, req.ps_hash0, req.ps_crc32);

		// If the driver failed to produce a pipeline, drop the request so the GPU
		// thread can fall back to synchronous compile on the next draw (rather than
		// spinning forever on a never-published pipeline). Remove the in-flight
		// marker under the lock and publish (or discard).
		std::exception_ptr eptr;
		bool produced = false;
		try {
			produced = (cached->pipeline != nullptr && cached->pipeline_layout != nullptr);
		} catch (...) {
			eptr = std::current_exception();
		}

		{
			std::scoped_lock lock(m_state_mutex);
			m_in_flight.erase(req.key);
		}

		if (eptr) {
			LOGF("AsyncPipelineCompiler: worker threw while building pipeline VS=0x%08x/0x%08x PS=0x%08x/0x%08x -- leaving for synchronous fallback\n",
			         static_cast<unsigned>(req.vs_hash0), static_cast<unsigned>(req.vs_crc32),
			         static_cast<unsigned>(req.ps_hash0), static_cast<unsigned>(req.ps_crc32));
			continue;
		}

		if (!produced) {
			LOGF("AsyncPipelineCompiler: vkCreateGraphicsPipelines did not produce a pipeline VS=0x%08x/0x%08x PS=0x%08x/0x%08x -- leaving for synchronous fallback\n",
			         static_cast<unsigned>(req.vs_hash0), static_cast<unsigned>(req.vs_crc32),
			         static_cast<unsigned>(req.ps_hash0), static_cast<unsigned>(req.ps_crc32));
			continue;
		}

		// Hand ownership to PipelineCache, which inserts it under m_mutex. If a
		// synchronous compile raced ahead and inserted the same key, Publish will
		// destroy our duplicate safely on this thread.
		m_owner.PublishCompiledPipeline(std::move(req.key), std::move(cached));
	}
}

} // namespace Libs::Graphics