#include "graphics/host_gpu/renderer/gdsBuffer.h"

#include "common/assert.h"
#include "graphics/host_gpu/transfer.h"
#include "graphics/host_gpu/vma.h"

namespace Libs::Graphics {

void GdsBuffer::Init() {
	if (m_buffer == nullptr) {
		m_buffer = std::make_unique<VulkanBuffer>();

		m_buffer->usage           = vk::BufferUsageFlagBits::eStorageBuffer;
		m_buffer->memory.property = vk::MemoryPropertyFlagBits::eHostVisible |
		                            vk::MemoryPropertyFlagBits::eHostCoherent |
		                            vk::MemoryPropertyFlagBits::eHostCached;
		m_graphics.CreateBuffer(DW_SIZE * 4, *m_buffer);
	}
}

void GdsBuffer::Clear(uint64_t dw_offset, uint32_t dw_num, uint32_t clear_value) {
	Common::LockGuard lock(m_mutex);

	Init();

	EXIT_NOT_IMPLEMENTED(dw_offset >= DW_SIZE);
	EXIT_NOT_IMPLEMENTED(dw_offset + dw_num > DW_SIZE);

	void* data = nullptr;
	m_graphics.MapMemory(m_buffer->memory, data);

	for (uint32_t i = 0; i < dw_num; i++) {
		static_cast<uint32_t*>(data)[dw_offset + i] = clear_value;
	}

	m_graphics.UnmapMemory(m_buffer->memory);
}

void GdsBuffer::Read(uint32_t* dst, uint32_t dw_offset, uint32_t dw_size) {
	EXIT_IF(dst == nullptr);

	Common::LockGuard lock(m_mutex);

	Init();

	EXIT_NOT_IMPLEMENTED(dw_offset >= DW_SIZE);
	EXIT_NOT_IMPLEMENTED(dw_offset + dw_size > DW_SIZE);

	void* data = nullptr;
	m_graphics.MapMemory(m_buffer->memory, data);

	for (uint32_t i = 0; i < dw_size; i++) {
		dst[i] = static_cast<uint32_t*>(data)[dw_offset + i];
	}

	m_graphics.UnmapMemory(m_buffer->memory);
}

VulkanBuffer& GdsBuffer::GetBuffer() {
	Common::LockGuard lock(m_mutex);

	Init();

	return *m_buffer;
}

} // namespace Libs::Graphics
