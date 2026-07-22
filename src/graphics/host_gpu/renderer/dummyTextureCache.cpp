#include "graphics/host_gpu/renderer/dummyTextureCache.h"

#include "common/assert.h"
#include "graphics/host_gpu/renderer/image.h"
#include "graphics/host_gpu/transfer.h"

#include <algorithm>

namespace Libs::Graphics {

namespace {

[[nodiscard]] constexpr size_t DummyTextureIndex(bool uint_format, bool image_3d) noexcept {
	return (image_3d ? 2u : 0u) + (uint_format ? 1u : 0u);
}

} // namespace

DummyTextureCache::~DummyTextureCache() {
	Common::LockGuard lock(m_mutex);
	const auto        populated = [](const auto& slots) {
		return std::ranges::any_of(slots, [](const auto& slot) { return slot.image != nullptr; });
	};
	if (!populated(m_sampled) && !populated(m_storage)) {
		return;
	}
	Transfer::WaitForQueueIdle();
	const auto destroy = [](auto& slots) {
		for (auto& slot: slots) {
			if (slot.image != nullptr) {
				ImageOps::Destroy(*slot.image);
				slot.image = nullptr;
			}
		}
	};
	destroy(m_sampled);
	destroy(m_storage);
}

VulkanImage& DummyTextureCache::Get(Usage usage, bool uint_format, bool image_3d) {
	Common::LockGuard lock(m_mutex);
	auto&             slots = usage == Usage::Storage ? m_storage : m_sampled;
	auto&             slot  = slots[DummyTextureIndex(uint_format, image_3d)];
	if (slot.image == nullptr) {
		slot.image = ImageOps::CreateDummyTexture(uint_format, image_3d, usage == Usage::Storage);
	}
	return *slot.image;
}

} // namespace Libs::Graphics
