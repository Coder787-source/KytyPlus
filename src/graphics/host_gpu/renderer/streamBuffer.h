#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_STREAMBUFFER_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_STREAMBUFFER_H_

#include "common/abi.h"
#include "common/common.h"

#include <cstdint>
#include <memory>
#include <mutex>

namespace Libs::Graphics {

struct GraphicContext;
struct VulkanBuffer;

// Host-only mapped stream mechanism. BufferCache owns the upload policy/API; one backend remains
// attached to each CommandBuffer so Reset is provably after that command's fence. Replacing this
// backend with scheduler watches will not change descriptor/render call sites.
class HostStreamBuffer final {
public:
	explicit HostStreamBuffer(GraphicContext& graphics);
	~HostStreamBuffer();
	KYTY_CLASS_NO_COPY(HostStreamBuffer);

	[[nodiscard]] bool Copy(const void* src, uint64_t size, uint64_t alignment,
	                        VulkanBuffer*& out_buffer, uint64_t& out_offset, uint64_t& out_range);
	void               Reset() noexcept;
	void               Release() noexcept;

private:
	void EnsureBuffer();

	// Interim fence-scoped backend: one command processor's eight live command buffers reserve up
	// to 256 MiB lazily (32 MiB each). Larger rings cut mid-frame upload stalls on iGPUs where
	// guest storage-buffer realign / uniform churn is heavy; the BufferCache seam stays fixed.
	static constexpr uint64_t CAPACITY = 32ull * 1024ull * 1024ull;

	mutable std::mutex            m_mutex;
	GraphicContext&               m_graphics;
	std::unique_ptr<VulkanBuffer> m_buffer;
	void*                         m_mapped = nullptr;
	uint64_t                      m_offset = 0;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_STREAMBUFFER_H_
