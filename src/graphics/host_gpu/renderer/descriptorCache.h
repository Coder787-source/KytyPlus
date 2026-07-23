#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DESCRIPTORCACHE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DESCRIPTORCACHE_H_

#include "common/abi.h"
#include "common/assert.h"
#include "common/common.h"
#include "common/threads.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/vulkanCommon.h"
#include "graphics/shader/shaderBindings.h"

#include <map>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace Libs::Graphics {

namespace ShaderRecompiler::IR {
struct Program;
struct ResourceSnapshot;
}

class CommandBuffer;
struct ShaderStageRuntime;

struct VulkanDescriptorSet {
	vk::DescriptorSet       set     = nullptr;
	vk::DescriptorSetLayout layout  = nullptr;
	int                     pool_id = -1;
};

struct BufferView {
	std::shared_ptr<VulkanBuffer> owner;
	VulkanBuffer*                 buffer = nullptr;
	vk::DeviceSize                offset = 0;
	vk::DeviceSize                range  = VK_WHOLE_SIZE;
	std::vector<uint8_t>          host_data;
};

class DescriptorCache {
public:
	enum class Stage { Unknown, Vertex, Pixel, Compute };

	struct TextureBinding {
		VulkanImage*  image      = nullptr;
		int           view       = VulkanImage::VIEW_DEFAULT;
		vk::ImageView image_view = nullptr;
		std::shared_ptr<void> owner;
	};

	enum class TextureVariant : int {
		Float2D = 0,
		Uint2D,
		FloatArray,
		UintArray,
		Float3D,
		Uint3D,
	};

	struct NativeDescriptors {
		std::vector<BufferView>     buffers;
		std::vector<TextureBinding> images;
		std::vector<vk::Sampler>    samplers;
		std::vector<BufferView>     addresses;
		BufferView                  gds;
		BufferView                  flattened_srt;
		BufferView                  user_data;
	};

	struct PreparedBindings {
		std::shared_ptr<const ShaderRecompiler::IR::Program> program;
		std::shared_ptr<const ShaderRecompiler::IR::ResourceSnapshot> snapshot;
		NativeDescriptors                                         resources;
		std::vector<uint32_t>                                     flattened_srt;
		std::vector<uint32_t>                                     user_data;
		vk::ShaderStageFlags                                      shader_stage;
		Stage                                                     stage = Stage::Unknown;
		bool                                                      committed = false;
	};

	explicit DescriptorCache(GraphicContext& graphics): m_graphics(graphics) {
		EXIT_NOT_IMPLEMENTED(!Common::Thread::IsMainThread());
	}
	~DescriptorCache() { KYTY_NOT_IMPLEMENTED; }
	KYTY_CLASS_NO_COPY(DescriptorCache);

	vk::DescriptorSetLayout GetDescriptorSetLayout(Stage                                stage,
	                                               const ShaderRecompiler::IR::Program& program);
	void                    Recycle(VulkanDescriptorSet& set);
	VulkanDescriptorSet&    GetDescriptor(Stage stage, const ShaderRecompiler::IR::Program& program,
	                                      const NativeDescriptors& descriptors);

private:
	struct Pool {
		vk::DescriptorPool pool           = nullptr;
		int                next_free_pool = -1;
	};

	void                 CreatePool();
	VulkanDescriptorSet* Allocate(Stage stage, const ShaderRecompiler::IR::Program& program);
	vk::DescriptorSetLayout
	GetDescriptorSetLayoutInternal(Stage stage, const ShaderRecompiler::IR::Program& program);

	GraphicContext&   m_graphics;
	Common::Mutex     m_mutex;
	std::vector<Pool> m_pools;
	int               m_first_free_pool = -1;
	std::unordered_map<vk::DescriptorSetLayout, std::vector<VulkanDescriptorSet*>>
	                                                         m_free_sets_by_layout;
	std::map<std::vector<uint32_t>, vk::DescriptorSetLayout> m_descriptor_set_layouts;
};

[[nodiscard]] DescriptorCache::PreparedBindings
PrepareBindings(CommandBuffer& buffer, const ShaderStageRuntime& runtime,
                vk::ShaderStageFlags shader_stage, DescriptorCache::Stage stage);
void RebindBuffers(CommandBuffer& buffer, DescriptorCache::PreparedBindings& bindings);
void RebindImages(CommandBuffer& buffer, DescriptorCache::PreparedBindings& bindings);
void ActivateImageWrites(std::span<DescriptorCache::PreparedBindings*> bindings);
void CommitBindings(CommandBuffer& buffer, vk::PipelineBindPoint pipeline_bind_point,
                    vk::PipelineLayout layout, DescriptorCache::PreparedBindings& bindings);

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DESCRIPTORCACHE_H_
