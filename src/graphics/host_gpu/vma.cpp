#include "graphics/host_gpu/vulkanCommon.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-private-field"
#pragma clang diagnostic ignored "-Wunused-variable"
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/vma.h"

#include <atomic>
#include <cinttypes>

namespace Libs::Graphics {

namespace {

struct MemoryStats {
	std::atomic_uint64_t allocated[VK_MAX_MEMORY_TYPES] {};
	std::atomic_uint64_t count[VK_MAX_MEMORY_TYPES] {};
};

MemoryStats g_memory_stats;

void TrackAllocationImpl(const VulkanMemory& memory) {
	g_memory_stats.allocated[memory.type] += memory.requirements.size;
	g_memory_stats.count[memory.type]++;
}

void UntrackAllocationImpl(const VulkanMemory& memory) {
	EXIT_IF(g_memory_stats.allocated[memory.type] < memory.requirements.size);
	EXIT_IF(g_memory_stats.count[memory.type] == 0);
	g_memory_stats.allocated[memory.type] -= memory.requirements.size;
	g_memory_stats.count[memory.type]--;
}

} // namespace

void VulkanTrackAllocation(const VulkanMemory& memory) {
	TrackAllocationImpl(memory);
}

void VulkanUntrackAllocation(const VulkanMemory& memory) {
	UntrackAllocationImpl(memory);
}

bool GraphicContext::CreateAllocator() {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(instance == nullptr || physical_device == nullptr || device == nullptr ||
	        allocator != nullptr);

	integrated_gpu =
	    physical_device_properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu;

	// On UMA iGPUs the Vulkan heap already points at system RAM. VK_EXT_memory_budget often
	// reports only the dedicated UMA carve-out (commonly 512 MiB–2 GiB), which makes VMA refuse
	// allocations long before the machine's 16–32 GiB of shared memory is exhausted. Keep the
	// extension enabled for logging on discrete GPUs; skip the VMA budget bit on integrated ones
	// so textures/buffers can grow into shared RAM (the practical "virtual VRAM" pool).
	memory_budget_for_vma = memory_budget_ext_enabled && !integrated_gpu;

	VmaVulkanFunctions functions {};
	functions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
	functions.vkGetDeviceProcAddr   = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo info {};
	info.instance         = instance;
	info.physicalDevice   = physical_device;
	info.device           = device;
	info.pVulkanFunctions = &functions;
	info.vulkanApiVersion = VULKAN_TARGET_API_VERSION;
	info.flags = memory_budget_for_vma ? VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT : 0;
	// Larger blocks cut allocation/map overhead when many guest textures are resident. 512 MiB is
	// cheap on a 32 GiB Mini PC / discrete card; keep the VMA default (256 MiB) only as a floor.
	info.preferredLargeHeapBlockSize = 512ull * 1024ull * 1024ull;

	const auto result = static_cast<vk::Result>(vmaCreateAllocator(&info, &allocator));
	if (result != vk::Result::eSuccess) {
		LOGF("vmaCreateAllocator failed: %s\n", VulkanToString(result).c_str());
		return false;
	}

	const auto& mem = GetPhysicalDeviceMemoryProperties();
	uint64_t    device_local_bytes = 0;
	for (uint32_t i = 0; i < mem.memoryHeapCount; i++) {
		if (mem.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
			device_local_bytes += mem.memoryHeaps[i].size;
		}
	}
	LOGF("VMA: device=%s type=%s memory_budget_ext=%d vma_uses_budget=%d "
	     "device_local_heaps=%" PRIu64 " MiB preferred_block=%" PRIu64 " MiB\n",
	     physical_device_properties.deviceName.data(),
	     integrated_gpu ? "integrated/UMA" : "discrete", memory_budget_ext_enabled ? 1 : 0,
	     memory_budget_for_vma ? 1 : 0, device_local_bytes / (1024ull * 1024ull),
	     info.preferredLargeHeapBlockSize / (1024ull * 1024ull));
	if (integrated_gpu) {
		LOGF("VMA: integrated GPU — allocations may use shared system RAM beyond the BIOS UMA "
		     "carve-out (no artificial 2 GiB VMA budget cap)\n");
	}
	return true;
}

void GraphicContext::DestroyAllocator() {
	if (allocator == nullptr) {
		return;
	}
	vmaDestroyAllocator(allocator);
	allocator = nullptr;
}

uint64_t VulkanNextMemoryUniqueId() {
	static std::atomic_uint64_t sequence = 0;
	return ++sequence;
}

void GraphicContext::LogMemoryBudget() const {
	if (allocator == nullptr || physical_device == nullptr) {
		return;
	}

	const auto& properties = GetPhysicalDeviceMemoryProperties();
	VmaBudget   budgets[VK_MAX_MEMORY_HEAPS] {};
	vmaGetHeapBudgets(allocator, budgets);
	for (uint32_t i = 0; i < properties.memoryHeapCount; i++) {
		LOGF("VMA heap %u: usage=%" PRIu64 ", budget=%" PRIu64 ", allocation=%" PRIu64
		     ", blocks=%" PRIu64 "\n",
		     i, static_cast<uint64_t>(budgets[i].usage), static_cast<uint64_t>(budgets[i].budget),
		     static_cast<uint64_t>(budgets[i].statistics.allocationBytes),
		     static_cast<uint64_t>(budgets[i].statistics.blockBytes));
	}
}

void GraphicContext::CreateBuffer(uint64_t size, VulkanBuffer& buffer) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(allocator == nullptr || buffer.buffer != nullptr ||
	        buffer.memory.allocation != nullptr || size == 0);

	vk::BufferCreateInfo buffer_info {};
	buffer_info.sType       = vk::StructureType::eBufferCreateInfo;
	buffer_info.size        = size;
	buffer_info.usage       = buffer.usage;
	buffer_info.sharingMode = vk::SharingMode::eExclusive;

	VmaAllocationCreateInfo alloc_info {};
	alloc_info.requiredFlags =
	    static_cast<vk::MemoryPropertyFlags::MaskType>(buffer.memory.property);
	alloc_info.preferredFlags =
	    static_cast<vk::MemoryPropertyFlags::MaskType>(buffer.memory.preferred_property);

	vk::Buffer::CType native_buffer = VK_NULL_HANDLE;
	const auto        result        = static_cast<vk::Result>(vmaCreateBuffer(
	    allocator, static_cast<const vk::BufferCreateInfo::NativeType*>(buffer_info), &alloc_info,
	    &native_buffer, &buffer.memory.allocation, &buffer.memory.allocation_info));
	buffer.buffer                   = native_buffer;
	if (result != vk::Result::eSuccess) {
		LogMemoryBudget();
	}
	EXIT_NOT_IMPLEMENTED(result != vk::Result::eSuccess);

	device.getBufferMemoryRequirements(buffer.buffer, &buffer.memory.requirements);
	buffer.memory.type      = buffer.memory.allocation_info.memoryType;
	buffer.memory.memory    = buffer.memory.allocation_info.deviceMemory;
	buffer.memory.offset    = buffer.memory.allocation_info.offset;
	buffer.memory.unique_id = VulkanNextMemoryUniqueId();
	buffer.buffer_size      = size;
	VulkanTrackAllocation(buffer.memory);
}

void GraphicContext::DeleteBuffer(VulkanBuffer& buffer) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(allocator == nullptr || buffer.buffer == nullptr ||
	        buffer.memory.allocation == nullptr);

	VulkanUntrackAllocation(buffer.memory);
	vmaDestroyBuffer(allocator, buffer.buffer, buffer.memory.allocation);
	buffer.buffer                 = nullptr;
	buffer.memory.memory          = nullptr;
	buffer.memory.allocation      = nullptr;
	buffer.memory.allocation_info = {};
	buffer.memory.offset          = 0;
}

bool GraphicContext::CreateImage(const vk::ImageCreateInfo& image_info, VulkanImage& image) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(allocator == nullptr || image.image != nullptr || image.memory.allocation != nullptr);

	auto&                   memory = image.memory;
	VmaAllocationCreateInfo alloc_info {};
	alloc_info.requiredFlags = static_cast<vk::MemoryPropertyFlags::MaskType>(memory.property);
	alloc_info.preferredFlags =
	    static_cast<vk::MemoryPropertyFlags::MaskType>(memory.preferred_property);

	vk::Image::CType native_image = VK_NULL_HANDLE;
	const auto       result       = static_cast<vk::Result>(
	    vmaCreateImage(allocator, static_cast<const vk::ImageCreateInfo::NativeType*>(image_info),
	                   &alloc_info, &native_image, &memory.allocation, &memory.allocation_info));
	image.image = native_image;
	if (result != vk::Result::eSuccess) {
		LogMemoryBudget();
		return false;
	}
	image.mutable_format =
	    static_cast<bool>(image_info.flags & vk::ImageCreateFlagBits::eMutableFormat);

	device.getImageMemoryRequirements(image.image, &memory.requirements);
	memory.type      = memory.allocation_info.memoryType;
	memory.memory    = memory.allocation_info.deviceMemory;
	memory.offset    = memory.allocation_info.offset;
	memory.unique_id = VulkanNextMemoryUniqueId();
	VulkanTrackAllocation(memory);
	return true;
}

void GraphicContext::DeleteImage(VulkanImage& image) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(allocator == nullptr || image.image == nullptr || image.memory.allocation == nullptr);

	auto& memory = image.memory;
	VulkanUntrackAllocation(memory);
	vmaDestroyImage(allocator, image.image, memory.allocation);
	image.image            = nullptr;
	memory.memory          = nullptr;
	memory.allocation      = nullptr;
	memory.allocation_info = {};
	memory.offset          = 0;
}

void GraphicContext::MapMemory(VulkanMemory& memory, void*& data) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(allocator == nullptr || memory.allocation == nullptr);
	EXIT_NOT_IMPLEMENTED(static_cast<vk::Result>(vmaMapMemory(allocator, memory.allocation,
	                                                          &data)) != vk::Result::eSuccess);
}

void GraphicContext::UnmapMemory(VulkanMemory& memory) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(allocator == nullptr || memory.allocation == nullptr);
	vmaUnmapMemory(allocator, memory.allocation);
}

} // namespace Libs::Graphics
