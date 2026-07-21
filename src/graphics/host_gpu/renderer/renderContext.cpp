#include "graphics/host_gpu/renderer/renderContext.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "kernel/pthread.h"
#include "libs/errno.h"

#include <algorithm>

namespace Libs::Graphics {

RenderContext::RenderContext(GraphicContext& graphics)
    : m_graphics(graphics), m_pipeline_cache(graphics), m_descriptor_cache(graphics),
      m_framebuffer_cache(graphics), m_sampler_cache(graphics), m_gds_buffer(graphics),
      m_gpu_resources(graphics) {
	EXIT_NOT_IMPLEMENTED(!Common::Thread::IsMainThread());
}

void RenderContext::AddEopEq(LibKernel::EventQueue::KernelEqueue eq, int id) {
	Common::LockGuard lock(m_eop_mutex);

	auto it = std::find_if(m_eop_eqs.begin(), m_eop_eqs.end(), [eq, id](const auto& entry) {
		return entry.eq == eq && entry.id == id;
	});
	if (it != m_eop_eqs.end()) {
		it->count++;
		return;
	}

	m_eop_eqs.push_back({eq, id, 1});
}

void RenderContext::DeleteEopEq(LibKernel::EventQueue::KernelEqueue eq, int id) {
	Common::LockGuard lock(m_eop_mutex);

	auto it = std::find_if(m_eop_eqs.begin(), m_eop_eqs.end(), [eq, id](const auto& entry) {
		return entry.eq == eq && entry.id == id;
	});
	if (it == m_eop_eqs.end()) {
		return;
	}

	if (--it->count == 0) {
		m_eop_eqs.erase(it);
	}
}

void RenderContext::TriggerEopEvent(uint32_t context_id) {
	Common::LockGuard lock(m_eop_mutex);

	for (auto& eop_entry: m_eop_eqs) {
		if (eop_entry.eq != nullptr) {
			const auto id     = static_cast<uintptr_t>(eop_entry.id);
			auto       result = LibKernel::EventQueue::KernelTriggerEvent(
			    eop_entry.eq, id, LibKernel::EventQueue::KERNEL_EVFILT_GRAPHICS,
			    reinterpret_cast<void*>(static_cast<uintptr_t>(context_id)));
			EXIT_NOT_IMPLEMENTED(result != OK && result != LibKernel::KERNEL_ERROR_ENOENT);
		}
	}

	auto tsc    = LibKernel::KernelReadTsc();
	auto result = LibKernel::EventQueue::KernelTriggerUserEventForAll(AGC_USER_INTERRUPT_EVENT,
	                                                                  reinterpret_cast<void*>(tsc));
	EXIT_NOT_IMPLEMENTED(result != OK && result != LibKernel::KERNEL_ERROR_ENOENT);
}

} // namespace Libs::Graphics
