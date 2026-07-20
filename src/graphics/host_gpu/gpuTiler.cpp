#include "graphics/host_gpu/gpuTiler.h"

#include "common/assert.h"
#include "common/threads.h"
#include "gpu_tiler_shaders/gpu_tiler_depth_spv.h"
#include "gpu_tiler_shaders/gpu_tiler_prt_3d_spv.h"
#include "gpu_tiler_shaders/gpu_tiler_prt_spv.h"
#include "gpu_tiler_shaders/gpu_tiler_render_target_spv.h"
#include "gpu_tiler_shaders/gpu_tiler_standard256_spv.h"
#include "gpu_tiler_shaders/gpu_tiler_standard4_3d_spv.h"
#include "gpu_tiler_shaders/gpu_tiler_standard4_spv.h"
#include "gpu_tiler_shaders/gpu_tiler_standard64_3d_spv.h"
#include "gpu_tiler_shaders/gpu_tiler_standard64_spv.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/host_gpu/vma.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <vector>

namespace Libs::Graphics {
namespace {

constexpr uint32_t GROUP_SIZE              = 64;
constexpr uint32_t FAMILY_COUNT            = static_cast<uint32_t>(TileBlockFamily::Count);
constexpr uint32_t BYTES_PER_ELEMENT_COUNT = 5;
constexpr uint32_t DIRECTION_COUNT         = 2;
constexpr uint32_t PIPELINE_COUNT = FAMILY_COUNT * BYTES_PER_ELEMENT_COUNT * DIRECTION_COUNT;
static_assert(FAMILY_COUNT == 9);

struct Push {
	uint32_t src_base;
	uint32_t dst_base;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t surface_z;
	uint32_t pitch_bytes;
	uint32_t slice_bytes;
	uint32_t blocks_per_row;
	uint32_t blocks_per_slice;
	uint32_t tail_x;
	uint32_t tail_y;
	uint32_t tail;
	uint32_t first;
	uint32_t count;
};
static_assert(sizeof(Push) == 60);

struct Shader {
	const uint32_t* code;
	size_t          words;
};

constexpr std::array<Shader, FAMILY_COUNT> SHADERS {{
    {GPU_TILER_STANDARD256_SPV, std::size(GPU_TILER_STANDARD256_SPV)},
    {GPU_TILER_STANDARD4_SPV, std::size(GPU_TILER_STANDARD4_SPV)},
    {GPU_TILER_STANDARD4_3D_SPV, std::size(GPU_TILER_STANDARD4_3D_SPV)},
    {GPU_TILER_STANDARD64_SPV, std::size(GPU_TILER_STANDARD64_SPV)},
    {GPU_TILER_STANDARD64_3D_SPV, std::size(GPU_TILER_STANDARD64_3D_SPV)},
    {GPU_TILER_PRT_SPV, std::size(GPU_TILER_PRT_SPV)},
    {GPU_TILER_PRT_3D_SPV, std::size(GPU_TILER_PRT_3D_SPV)},
    {GPU_TILER_RENDER_TARGET_SPV, std::size(GPU_TILER_RENDER_TARGET_SPV)},
    {GPU_TILER_DEPTH_SPV, std::size(GPU_TILER_DEPTH_SPV)},
}};

struct Dispatch {
	Push     push {};
	uint32_t pipeline_slot = 0;
	uint32_t elements      = 0;
};

struct Resources {
	vk::DescriptorSetLayout                  descriptor_layout = nullptr;
	vk::PipelineLayout                       pipeline_layout   = nullptr;
	vk::DescriptorPool                       descriptor_pool   = nullptr;
	vk::DescriptorSet                        descriptor_set    = nullptr;
	std::array<vk::Pipeline, PIPELINE_COUNT> pipelines {};
	VulkanBuffer                             staging;
	VulkanBuffer                             linear;
	void*                                    mapped = nullptr;
};

bool CheckedAdd(uint64_t a, uint64_t b, uint64_t* result) {
	return b <= UINT64_MAX - a && (*result = a + b, true);
}

bool CheckedMultiply(uint64_t a, uint64_t b, uint64_t* result) {
	return (a == 0 || b <= UINT64_MAX / a) && (*result = a * b, true);
}

bool CheckedAddProduct(uint64_t* value, uint64_t count, uint64_t stride) {
	uint64_t bytes = 0;
	return CheckedMultiply(count, stride, &bytes) && CheckedAdd(*value, bytes, value);
}

bool IsRangeValid(uint64_t offset, uint64_t size, uint64_t capacity) {
	return size != 0 && offset <= capacity && size <= capacity - offset;
}

uint64_t AlignToDword(uint64_t value) {
	return (value + 3u) & ~uint64_t {3};
}

uint32_t GetPipelineSlot(bool to_tiled, TileBlockFamily family, uint32_t bytes_per_element) {
	const uint32_t direction_index    = to_tiled ? 1u : 0u;
	const uint32_t family_index       = static_cast<uint32_t>(family);
	const uint32_t element_size_index = std::countr_zero(bytes_per_element);
	return (direction_index * FAMILY_COUNT + family_index) * BYTES_PER_ELEMENT_COUNT +
	       element_size_index;
}

void Barrier(vk::CommandBuffer command, vk::Buffer buffer, vk::AccessFlags src_access,
             vk::AccessFlags dst_access, vk::PipelineStageFlags src_stage,
             vk::PipelineStageFlags dst_stage) {
	vk::BufferMemoryBarrier barrier {};
	barrier.sType               = vk::StructureType::eBufferMemoryBarrier;
	barrier.srcAccessMask       = src_access;
	barrier.dstAccessMask       = dst_access;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer              = buffer;
	barrier.size                = VK_WHOLE_SIZE;
	command.pipelineBarrier(src_stage, dst_stage, {}, 0, nullptr, 1, &barrier, 0, nullptr);
}

class Tiler final {
public:
	void Run(bool to_tiled, GraphicContext* context, const void* input, void* output,
	         uint64_t tiled_capacity, uint64_t linear_capacity, std::span<const GpuTileInfo> infos,
	         const GpuTileRecord& record);
	void Release(GraphicContext* context);

private:
	void Prepare(bool to_tiled, GraphicContext* context, uint64_t tiled_capacity,
	             uint64_t linear_capacity, std::span<const GpuTileInfo> infos,
	             std::vector<Dispatch>* dispatches) const;
	void Init(GraphicContext* context);
	void CreatePipelines(std::span<const Dispatch> dispatches);
	void CreatePipeline(uint32_t pipeline_slot);
	void Resize(uint64_t staging_size, uint64_t linear_size);
	void CreateBuffer(uint64_t size, bool mapped, VulkanBuffer* buffer, void** data) const;
	void Execute(bool to_tiled, const void* input, void* output, uint64_t tiled_capacity,
	             uint64_t linear_capacity, std::span<const Dispatch> dispatches,
	             const GpuTileRecord& record);
	void Destroy(Resources* target) const;

	Common::Mutex   mutex;
	GraphicContext* ctx = nullptr;
	Resources       resources;
};

Tiler g_tiler;

void Tiler::Prepare(bool to_tiled, GraphicContext* context, uint64_t tiled_capacity,
                    uint64_t linear_capacity, std::span<const GpuTileInfo> infos,
                    std::vector<Dispatch>* dispatches) const {
	EXIT_IF(context == nullptr || g_render_ctx == nullptr ||
	        g_render_ctx->GetGraphicCtx() != context || infos.empty() || tiled_capacity == 0 ||
	        linear_capacity == 0);
	const auto& limits = context->GetPhysicalDeviceProperties().limits;
	EXIT_NOT_IMPLEMENTED(tiled_capacity > UINT32_MAX || linear_capacity > UINT32_MAX ||
	                     AlignToDword(tiled_capacity) > limits.maxStorageBufferRange ||
	                     AlignToDword(linear_capacity) > limits.maxStorageBufferRange);

	dispatches->clear();
	dispatches->reserve(infos.size());
	for (const auto& info: infos) {
		TileBlockLayout block {};
		const uint32_t  tiled_width  = info.tiled_width != 0 ? info.tiled_width : info.pitch;
		const uint32_t  tiled_height = info.tiled_height != 0 ? info.tiled_height : info.height;
		EXIT_NOT_IMPLEMENTED(
		    !TileGetBlockLayout(info.family, info.bytes_per_element, &block) || info.width == 0 ||
		    info.height == 0 || info.depth == 0 || info.pitch < info.width ||
		    (!info.tail && (tiled_width < info.width || tiled_height < info.height)) ||
		    !IsRangeValid(info.linear_offset, info.linear_size, linear_capacity) ||
		    !IsRangeValid(info.tiled_offset, info.tiled_size, tiled_capacity) ||
		    (block.block_depth == 1 && info.depth != 1));

		uint64_t elements = 0, pitch_bytes = 0;
		EXIT_NOT_IMPLEMENTED(!CheckedMultiply(info.width, info.height, &elements) ||
		                     !CheckedMultiply(elements, info.depth, &elements) ||
		                     !CheckedMultiply(info.pitch, info.bytes_per_element, &pitch_bytes) ||
		                     elements > UINT32_MAX || pitch_bytes > UINT32_MAX);
		uint64_t slice_bytes = info.linear_slice_stride;
		EXIT_NOT_IMPLEMENTED(slice_bytes == 0 &&
		                     !CheckedMultiply(pitch_bytes, info.height, &slice_bytes));
		uint64_t linear_used = 0, minimum_slice = 0;
		EXIT_NOT_IMPLEMENTED(!CheckedMultiply(pitch_bytes, info.height, &minimum_slice) ||
		                     (info.depth > 1 && slice_bytes < minimum_slice) ||
		                     !CheckedAddProduct(&linear_used, info.depth - 1u, slice_bytes) ||
		                     !CheckedAddProduct(&linear_used, info.height - 1u, pitch_bytes) ||
		                     !CheckedAddProduct(&linear_used, info.width, info.bytes_per_element) ||
		                     linear_used > info.linear_size || slice_bytes > UINT32_MAX);

		const uint64_t columns =
		    (static_cast<uint64_t>(tiled_width) + block.block_width - 1u) / block.block_width;
		const uint64_t rows =
		    (static_cast<uint64_t>(tiled_height) + block.block_height - 1u) / block.block_height;
		uint64_t blocks_per_slice = 0;
		EXIT_NOT_IMPLEMENTED(!CheckedMultiply(columns, rows, &blocks_per_slice) ||
		                     columns > UINT32_MAX || blocks_per_slice > UINT32_MAX ||
		                     rows * block.block_height > UINT32_MAX);
		if (info.tail) {
			const bool supported = info.family != TileBlockFamily::Standard256B;
			EXIT_NOT_IMPLEMENTED(
			    !supported || info.depth > block.block_depth || info.tail_x >= block.block_width ||
			    info.width > block.block_width - info.tail_x || info.tail_y >= block.block_height ||
			    info.height > block.block_height - info.tail_y ||
			    info.tiled_size < block.block_size);
		} else {
			const uint64_t slices =
			    (static_cast<uint64_t>(info.depth) + block.block_depth - 1u) / block.block_depth;
			uint64_t tiled_used = 0;
			EXIT_NOT_IMPLEMENTED(!CheckedMultiply(blocks_per_slice, slices, &tiled_used) ||
			                     !CheckedMultiply(tiled_used, block.block_size, &tiled_used) ||
			                     tiled_used > info.tiled_size);
		}
		const uint32_t alignment = std::min(info.bytes_per_element, 4u);
		EXIT_NOT_IMPLEMENTED(((info.linear_offset | info.tiled_offset | pitch_bytes | slice_bytes) &
		                      (alignment - 1u)) != 0);

		Dispatch dispatch {};
		dispatch.elements      = static_cast<uint32_t>(elements);
		dispatch.pipeline_slot = GetPipelineSlot(to_tiled, info.family, info.bytes_per_element);
		dispatch.push.src_base =
		    static_cast<uint32_t>(to_tiled ? info.linear_offset : info.tiled_offset);
		dispatch.push.dst_base =
		    static_cast<uint32_t>(to_tiled ? info.tiled_offset : info.linear_offset);
		dispatch.push.width            = info.width;
		dispatch.push.height           = info.height;
		dispatch.push.depth            = info.depth;
		dispatch.push.surface_z        = info.surface_z;
		dispatch.push.pitch_bytes      = static_cast<uint32_t>(pitch_bytes);
		dispatch.push.slice_bytes      = static_cast<uint32_t>(slice_bytes);
		dispatch.push.blocks_per_row   = static_cast<uint32_t>(columns);
		dispatch.push.blocks_per_slice = static_cast<uint32_t>(blocks_per_slice);
		dispatch.push.tail_x           = info.tail_x;
		dispatch.push.tail_y           = info.tail_y;
		dispatch.push.tail             = info.tail;
		dispatches->push_back(dispatch);
	}
}

void Tiler::Destroy(Resources* target) const {
	if (ctx == nullptr) {
		return;
	}
	if (target->mapped != nullptr) {
		VulkanUnmapMemory(ctx, &target->staging.memory);
	}
	if (target->staging.buffer != nullptr) {
		VulkanDeleteBuffer(ctx, &target->staging);
	}
	if (target->linear.buffer != nullptr) {
		VulkanDeleteBuffer(ctx, &target->linear);
	}
	for (auto pipeline: target->pipelines) {
		if (pipeline != nullptr) {
			ctx->device.destroyPipeline(pipeline, nullptr);
		}
	}
	if (target->descriptor_pool != nullptr) {
		ctx->device.destroyDescriptorPool(target->descriptor_pool, nullptr);
	}
	if (target->pipeline_layout != nullptr) {
		ctx->device.destroyPipelineLayout(target->pipeline_layout, nullptr);
	}
	if (target->descriptor_layout != nullptr) {
		ctx->device.destroyDescriptorSetLayout(target->descriptor_layout, nullptr);
	}
	*target = {};
}

void Tiler::Init(GraphicContext* context) {
	if (resources.pipeline_layout != nullptr) {
		EXIT_IF(ctx != context);
		return;
	}
	EXIT_IF(context == nullptr || context->device == nullptr || context->allocator == nullptr);
	ctx = context;
	std::array<vk::DescriptorSetLayoutBinding, 2> bindings {};
	for (uint32_t i = 0; i < bindings.size(); i++) {
		bindings[i] = {i, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute,
		               nullptr};
	}
	vk::DescriptorSetLayoutCreateInfo descriptor_info {};
	descriptor_info.sType        = vk::StructureType::eDescriptorSetLayoutCreateInfo;
	descriptor_info.bindingCount = static_cast<uint32_t>(bindings.size());
	descriptor_info.pBindings    = bindings.data();
	RequireVulkanSuccess(ctx->device.createDescriptorSetLayout(&descriptor_info, nullptr,
	                                                           &resources.descriptor_layout),
	                     "create GPU tiler descriptor layout");

	vk::PushConstantRange        push_range {vk::ShaderStageFlagBits::eCompute, 0, sizeof(Push)};
	vk::PipelineLayoutCreateInfo layout_info {};
	layout_info.sType                  = vk::StructureType::ePipelineLayoutCreateInfo;
	layout_info.setLayoutCount         = 1;
	layout_info.pSetLayouts            = &resources.descriptor_layout;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges    = &push_range;
	RequireVulkanSuccess(
	    ctx->device.createPipelineLayout(&layout_info, nullptr, &resources.pipeline_layout),
	    "create GPU tiler pipeline layout");
	vk::DescriptorPoolSize       pool_size {vk::DescriptorType::eStorageBuffer, 2};
	vk::DescriptorPoolCreateInfo pool_info {};
	pool_info.sType         = vk::StructureType::eDescriptorPoolCreateInfo;
	pool_info.maxSets       = 1;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes    = &pool_size;
	RequireVulkanSuccess(
	    ctx->device.createDescriptorPool(&pool_info, nullptr, &resources.descriptor_pool),
	    "create GPU tiler descriptor pool");
	vk::DescriptorSetAllocateInfo set_info {};
	set_info.sType              = vk::StructureType::eDescriptorSetAllocateInfo;
	set_info.descriptorPool     = resources.descriptor_pool;
	set_info.descriptorSetCount = 1;
	set_info.pSetLayouts        = &resources.descriptor_layout;
	RequireVulkanSuccess(ctx->device.allocateDescriptorSets(&set_info, &resources.descriptor_set),
	                     "allocate GPU tiler descriptor set");
}

void Tiler::CreatePipeline(uint32_t pipeline_slot) {
	const uint32_t element_size_index     = pipeline_slot % BYTES_PER_ELEMENT_COUNT;
	const uint32_t family_direction_index = pipeline_slot / BYTES_PER_ELEMENT_COUNT;
	const uint32_t family_index           = family_direction_index % FAMILY_COUNT;
	const uint32_t direction_index        = family_direction_index / FAMILY_COUNT;
	const uint32_t specialization_values[] {1u << element_size_index, direction_index};
	const vk::SpecializationMapEntry entries[] {{0, 0, 4}, {1, 4, 4}};
	vk::SpecializationInfo           specialization {2, entries, sizeof(specialization_values),
	                                                 specialization_values};
	vk::ShaderModuleCreateInfo       module_info {};
	module_info.sType       = vk::StructureType::eShaderModuleCreateInfo;
	module_info.codeSize    = SHADERS[family_index].words * sizeof(uint32_t);
	module_info.pCode       = SHADERS[family_index].code;
	vk::ShaderModule module = nullptr;
	RequireVulkanSuccess(ctx->device.createShaderModule(&module_info, nullptr, &module),
	                     "create GPU tiler shader module");
	vk::PipelineShaderStageCreateInfo stage {};
	stage.sType               = vk::StructureType::ePipelineShaderStageCreateInfo;
	stage.stage               = vk::ShaderStageFlagBits::eCompute;
	stage.module              = module;
	stage.pName               = "main";
	stage.pSpecializationInfo = &specialization;
	vk::ComputePipelineCreateInfo info {};
	info.sType            = vk::StructureType::eComputePipelineCreateInfo;
	info.stage            = stage;
	info.layout           = resources.pipeline_layout;
	vk::Pipeline pipeline = nullptr;
	const auto   result = ctx->device.createComputePipelines(nullptr, 1, &info, nullptr, &pipeline);
	ctx->device.destroyShaderModule(module, nullptr);
	RequireVulkanSuccess(result, "create GPU tiler pipeline");
	resources.pipelines[pipeline_slot] = pipeline;
}

void Tiler::CreatePipelines(std::span<const Dispatch> dispatches) {
	for (const auto& dispatch: dispatches) {
		if (resources.pipelines[dispatch.pipeline_slot] == nullptr) {
			CreatePipeline(dispatch.pipeline_slot);
		}
	}
}

void Tiler::CreateBuffer(uint64_t size, bool mapped, VulkanBuffer* buffer, void** data) const {
	buffer->usage = vk::BufferUsageFlagBits::eStorageBuffer |
	                vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
	buffer->memory.property =
	    mapped
	        ? vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	        : vk::MemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal);
	VulkanCreateBuffer(ctx, size, buffer);
	if (mapped) VulkanMapMemory(ctx, &buffer->memory, data);
}

void Tiler::Resize(uint64_t staging_size, uint64_t linear_size) {
	if (resources.staging.buffer_size >= staging_size &&
	    resources.linear.buffer_size >= linear_size) {
		return;
	}
	staging_size = std::max(staging_size, resources.staging.buffer_size);
	linear_size  = std::max(linear_size, resources.linear.buffer_size);
	VulkanBuffer staging {}, linear {};
	void*        mapped = nullptr;
	CreateBuffer(staging_size, true, &staging, &mapped);
	CreateBuffer(linear_size, false, &linear, nullptr);
	if (resources.mapped != nullptr) VulkanUnmapMemory(ctx, &resources.staging.memory);
	if (resources.staging.buffer != nullptr) VulkanDeleteBuffer(ctx, &resources.staging);
	if (resources.linear.buffer != nullptr) VulkanDeleteBuffer(ctx, &resources.linear);
	resources.staging = staging;
	resources.linear  = linear;
	resources.mapped  = mapped;
}

void Tiler::Execute(bool to_tiled, const void* input, void* output, uint64_t tiled_capacity,
                    uint64_t linear_capacity, std::span<const Dispatch> dispatches,
                    const GpuTileRecord& record) {
	const uint64_t tiled_size  = AlignToDword(tiled_capacity);
	const uint64_t linear_size = AlignToDword(linear_capacity);
	const uint64_t input_size  = to_tiled ? linear_capacity : tiled_capacity;
	if (input != nullptr) {
		std::memcpy(resources.mapped, input, static_cast<size_t>(input_size));
		std::memset(static_cast<uint8_t*>(resources.mapped) + input_size, 0,
		            static_cast<size_t>(AlignToDword(input_size) - input_size));
	}

	std::array<vk::DescriptorBufferInfo, 2> buffer_info {{
	    {to_tiled ? resources.linear.buffer : resources.staging.buffer, 0,
	     to_tiled ? linear_size : tiled_size},
	    {to_tiled ? resources.staging.buffer : resources.linear.buffer, 0,
	     to_tiled ? tiled_size : linear_size},
	}};
	std::array<vk::WriteDescriptorSet, 2>   writes {};
	for (uint32_t i = 0; i < writes.size(); i++) {
		writes[i].sType           = vk::StructureType::eWriteDescriptorSet;
		writes[i].dstSet          = resources.descriptor_set;
		writes[i].dstBinding      = i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType  = vk::DescriptorType::eStorageBuffer;
		writes[i].pBufferInfo     = &buffer_info[i];
	}
	ctx->device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0,
	                                 nullptr);

	CommandBuffer command(GraphicContext::QUEUE_UTIL);
	command.Begin();
	auto vk_command = command.Handle();
	if (input != nullptr) {
		Barrier(vk_command, resources.staging.buffer, vk::AccessFlagBits::eHostWrite,
		        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferRead,
		        vk::PipelineStageFlagBits::eHost,
		        vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer);
	}
	if (to_tiled && input != nullptr) {
		const vk::BufferCopy copy {0, 0, linear_size};
		vk_command.copyBuffer(resources.staging.buffer, resources.linear.buffer, 1, &copy);
		Barrier(vk_command, resources.linear.buffer, vk::AccessFlagBits::eTransferWrite,
		        vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eTransfer,
		        vk::PipelineStageFlagBits::eComputeShader);
		Barrier(vk_command, resources.staging.buffer, vk::AccessFlagBits::eTransferRead,
		        vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer,
		        vk::PipelineStageFlagBits::eTransfer);
	}
	if (to_tiled && record) {
		record(&command, &resources.linear);
		Barrier(vk_command, resources.linear.buffer,
		        vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eMemoryWrite,
		        vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eAllCommands,
		        vk::PipelineStageFlagBits::eComputeShader);
	}

	const auto output_buffer = to_tiled ? resources.staging.buffer : resources.linear.buffer;
	const auto output_size   = to_tiled ? tiled_size : linear_size;
	vk_command.fillBuffer(output_buffer, 0, output_size, 0);
	Barrier(vk_command, output_buffer, vk::AccessFlagBits::eTransferWrite,
	        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
	        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader);
	vk_command.bindDescriptorSets(vk::PipelineBindPoint::eCompute, resources.pipeline_layout, 0, 1,
	                              &resources.descriptor_set, 0, nullptr);
	const uint64_t limit =
	    static_cast<uint64_t>(
	        ctx->GetPhysicalDeviceProperties().limits.maxComputeWorkGroupCount[0]) *
	    GROUP_SIZE;
	for (const auto& dispatch: dispatches) {
		vk_command.bindPipeline(vk::PipelineBindPoint::eCompute,
		                        resources.pipelines[dispatch.pipeline_slot]);
		for (uint32_t first = 0; first < dispatch.elements;) {
			auto push  = dispatch.push;
			push.first = first;
			push.count =
			    static_cast<uint32_t>(std::min<uint64_t>(dispatch.elements - first, limit));
			vk_command.pushConstants(resources.pipeline_layout, vk::ShaderStageFlagBits::eCompute,
			                         0, sizeof(push), &push);
			vk_command.dispatch((push.count - 1u) / GROUP_SIZE + 1u, 1, 1);
			first += push.count;
		}
	}
	Barrier(vk_command, output_buffer, vk::AccessFlagBits::eShaderWrite,
	        vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eHostRead,
	        vk::PipelineStageFlagBits::eComputeShader,
	        vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eHost);
	if (!to_tiled && record) {
		record(&command, &resources.linear);
	}
	if (!to_tiled && output != nullptr) {
		const vk::BufferCopy copy {0, 0, linear_size};
		vk_command.copyBuffer(resources.linear.buffer, resources.staging.buffer, 1, &copy);
		Barrier(vk_command, resources.staging.buffer, vk::AccessFlagBits::eTransferWrite,
		        vk::AccessFlagBits::eHostRead, vk::PipelineStageFlagBits::eTransfer,
		        vk::PipelineStageFlagBits::eHost);
	}
	command.End();
	command.Execute();
	command.WaitForFence();
	if (output != nullptr) {
		std::memcpy(output, resources.mapped,
		            static_cast<size_t>(to_tiled ? tiled_capacity : linear_capacity));
	}
}

void Tiler::Run(bool to_tiled, GraphicContext* context, const void* input, void* output,
                uint64_t tiled_capacity, uint64_t linear_capacity,
                std::span<const GpuTileInfo> infos, const GpuTileRecord& record) {
	Common::LockGuard lock(mutex);
	EXIT_IF((to_tiled && (output == nullptr || (input == nullptr && !record))) ||
	        (!to_tiled && (input == nullptr || (output == nullptr && !record))));
	std::vector<Dispatch> dispatches;
	Prepare(to_tiled, context, tiled_capacity, linear_capacity, infos, &dispatches);
	Init(context);
	CreatePipelines(dispatches);
	const uint64_t staging_size =
	    std::max(AlignToDword(tiled_capacity), AlignToDword(linear_capacity));
	const uint64_t linear_size = AlignToDword(linear_capacity);
	Resize(staging_size, linear_size);
	Execute(to_tiled, input, output, tiled_capacity, linear_capacity, dispatches, record);
}

void Tiler::Release(GraphicContext* context) {
	Common::LockGuard lock(mutex);
	EXIT_IF(ctx != nullptr && context != ctx);
	Destroy(&resources);
	ctx = nullptr;
}

} // namespace

void GpuDetile(GraphicContext* ctx, const void* tiled, void* linear, uint64_t tiled_capacity,
               uint64_t linear_capacity, std::span<const GpuTileInfo> infos,
               const GpuTileRecord& after) {
	g_tiler.Run(false, ctx, tiled, linear, tiled_capacity, linear_capacity, infos, after);
}

void GpuTile(GraphicContext* ctx, const void* linear, void* tiled, uint64_t tiled_capacity,
             uint64_t linear_capacity, std::span<const GpuTileInfo> infos,
             const GpuTileRecord& before) {
	g_tiler.Run(true, ctx, linear, tiled, tiled_capacity, linear_capacity, infos, before);
}

void GpuTileRelease(GraphicContext* ctx) {
	g_tiler.Release(ctx);
}

} // namespace Libs::Graphics
