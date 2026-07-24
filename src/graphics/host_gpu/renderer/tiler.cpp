#include "graphics/host_gpu/renderer/tiler.h"

#include "common/assert.h"
#include "graphics/guest_gpu/gpu_defs.h"
#include "graphics/guest_gpu/gpu_format.h"
#include "graphics/guest_gpu/tile.h"
#include "graphics/host_gpu/gpuTiler.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/objects/textureCommon.h"
#include "graphics/host_gpu/renderer/image.h"
#include "graphics/host_gpu/transfer.h"

#include <vector>

namespace Libs::Graphics {
namespace {

struct DepthTransfer {
	std::vector<GpuTileInfo>     infos;
	std::vector<BufferImageCopy> regions;
};

DepthTransfer MakeDepthTransfer(uint64_t size, uint32_t layers, uint32_t format,
                                uint32_t bytes_per_element, uint32_t width, uint32_t height,
                                uint32_t pitch, uint32_t base_layer, vk::ImageAspectFlags aspect) {
	EXIT_IF(size == 0 || layers == 0 || size % layers != 0);
	TileBlockLayout block {};
	EXIT_NOT_IMPLEMENTED(
	    !TileGetBlockLayout(TileBlockFamily::Depth64KB, bytes_per_element, block) ||
	    Prospero::NumBytesPerElement(format) != bytes_per_element);

	const uint64_t slice_size = size / layers;
	DepthTransfer  transfer;
	transfer.infos.reserve(layers);
	transfer.regions.reserve(layers);
	for (uint32_t layer = 0; layer < layers; layer++) {
		const uint64_t offset = slice_size * layer;
		GpuTileInfo    info {block.family,
		                     block.bytes_per_element,
		                     offset,
		                     slice_size,
		                     offset,
		                     slice_size,
		                     0,
		                     width,
		                     height,
		                     1,
		                     pitch};
		info.surface_z = base_layer + layer;
		transfer.infos.push_back(info);

		BufferImageCopy region {};
		region.offset    = static_cast<uint32_t>(offset);
		region.pitch     = pitch;
		region.width     = width;
		region.height    = height;
		region.dst_layer = base_layer + layer;
		region.aspect    = aspect;
		transfer.regions.push_back(region);
	}
	return transfer;
}

void UploadDepth(DepthStencilVulkanImage& image, uint64_t source_address, uint64_t size,
                 uint32_t layers, uint32_t format, uint32_t bytes_per_element, uint32_t width,
                 uint32_t height, uint32_t pitch, uint32_t base_layer,
                 vk::ImageAspectFlags aspect) {
	auto transfer = MakeDepthTransfer(size, layers, format, bytes_per_element, width, height, pitch,
	                                  base_layer, aspect);
	Transfer::UploadTiledImage(image, reinterpret_cast<const void*>(source_address), size, size,
	                           transfer.infos, transfer.regions,
	                           vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

template <uint32_t (*Encode)(uint16_t)>
void UploadPromotedD16Depth(DepthStencilVulkanImage& image, const DepthTargetInfo& info,
                            const BufferImageCopySource& source, uint32_t base_layer) {
	const uint64_t guest_slice_size = info.size / info.layers;
	const uint64_t texels           = static_cast<uint64_t>(info.pitch) * info.height;
	const uint64_t host_slice_size  = texels * sizeof(uint32_t);
	const uint64_t host_upload_size = host_slice_size * info.layers;
	EXIT_IF(host_upload_size > UINT32_MAX);

	auto transfer = MakeDepthTransfer(info.size, info.layers, info.guest_format,
	                                  info.bytes_per_element, info.width, info.height, info.pitch,
	                                  base_layer, vk::ImageAspectFlagBits::eDepth);
	std::vector<uint16_t> guest_linear(info.size / sizeof(uint16_t));
	GpuDetile(reinterpret_cast<const void*>(source.address), guest_linear.data(), info.size,
	          info.size, transfer.infos);

	Transfer::ScratchBuffer host_linear(host_upload_size);
	for (uint32_t layer = 0; layer < info.layers; layer++) {
		const auto* guest = guest_linear.data() + guest_slice_size / sizeof(uint16_t) * layer;
		auto*       host  = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(host_linear.Data()) +
		                                                host_slice_size * layer);
		for (uint64_t texel = 0; texel < texels; texel++) {
			host[texel] = Encode(guest[texel]);
		}
		transfer.regions[layer].offset = static_cast<uint32_t>(host_slice_size * layer);
	}
	Transfer::UploadImage(image, host_linear.Data(), host_upload_size, transfer.regions,
	                      vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

} // namespace

void Tiler::DetileImage(GpuTextureVulkanImage& image, const ImageInfo& info,
                        const BufferImageCopySource& source, bool refresh, bool storage) const {
	if (refresh) Transfer::WaitForQueueIdle();

	const bool array_texture  = TextureIsLayeredTexture(info.type);
	const bool volume_texture = TextureIs3DTexture(info.type);
	auto       layout         = TextureCalcUploadLayout(
	    info.format, info.width, info.height, info.levels, info.depth, info.pitch, info.tile,
	    info.size, true, volume_texture, storage ? "StorageTextureCache" : "TextureCache");
	auto regions = TextureBuildUploadRegions(layout, image.format, info.width, info.height,
	                                         info.depth, info.levels, array_texture, volume_texture,
	                                         TextureUploadDestination::MipLevels);
	TextureUploadGuestImage(image, reinterpret_cast<const void*>(source.address), info.size,
	                        regions, layout, info.format, info.width, info.height, info.depth,
	                        info.levels, storage ? "StorageTextureCache" : "TextureCache",
	                        storage ? vk::ImageLayout::eGeneral
	                                : vk::ImageLayout::eShaderReadOnlyOptimal);
}

void Tiler::DetileImage(DepthStencilVulkanImage& image, const DepthTargetInfo& info,
                        const BufferImageCopySource& source, bool refresh,
                        uint32_t base_layer) const {
	EXIT_NOT_IMPLEMENTED(info.samples != 1 || image.samples != 1);
	if (refresh) Transfer::WaitForQueueIdle();

	if (DepthAspectTransferBytes(info.format) != info.bytes_per_element) {
		switch (info.format) {
			case vk::Format::eD24UnormS8Uint:
				UploadPromotedD16Depth<EncodeD16AsD24>(image, info, source, base_layer);
				return;
			case vk::Format::eD32SfloatS8Uint:
				UploadPromotedD16Depth<EncodeD16AsD32>(image, info, source, base_layer);
				return;
			default: EXIT_NOT_IMPLEMENTED(true);
		}
	}
	UploadDepth(image, source.address, info.size, info.layers, info.guest_format,
	            info.bytes_per_element, info.width, info.height, info.pitch, base_layer,
	            vk::ImageAspectFlagBits::eDepth);
}

void Tiler::DetileStencil(DepthStencilVulkanImage& image, const DepthTargetInfo& info,
                          const BufferImageCopySource& source, bool refresh,
                          uint32_t base_layer) const {
	EXIT_NOT_IMPLEMENTED(info.samples != 1 || image.samples != 1);
	if (refresh) Transfer::WaitForQueueIdle();

	const auto format = Prospero::GpuEnumValue(Prospero::BufferFormat::k8UInt);
	const auto pitch  = TileGetTexturePitch(format, info.width, 1,
	                                        Prospero::GpuEnumValue(Prospero::TileMode::kDepth));
	UploadDepth(image, source.address, info.stencil_size, info.layers, format, 1, info.width,
	            info.height, pitch, base_layer, vk::ImageAspectFlagBits::eStencil);
}

} // namespace Libs::Graphics
