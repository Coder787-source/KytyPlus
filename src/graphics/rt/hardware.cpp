#include "graphics/rt/hardware.h"

#include "common/assert.h"
#include "common/logging/log.h"

#include <algorithm>
#include <cstring>

namespace Libs::Graphics {
namespace {

bool ExtensionAvailable(const std::vector<vk::ExtensionProperties>& available, const char* name) {
	return std::ranges::any_of(available, [name](const auto& extension) {
		return std::strcmp(extension.extensionName, name) == 0;
	});
}

void AppendExtensionIfMissing(std::vector<const char*>& extensions, const char* name) {
	if (std::ranges::none_of(extensions, [name](const char* extension) {
		    return std::strcmp(extension, name) == 0;
	    })) {
		extensions.push_back(name);
	}
}

} // namespace

void GraphicContext::AppendHardwareRayTracingDeviceExtensions(
    const std::vector<vk::ExtensionProperties>& available_extensions,
    std::vector<const char*>&                   device_extensions) {

	const char* required[] = {
	    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	    VK_KHR_RAY_QUERY_EXTENSION_NAME,
	};
	for (const auto* extension: required) {
		if (!ExtensionAvailable(available_extensions, extension)) {
			LOGF("Vulkan RT: extension %s is unavailable; hardware ray tracing disabled\n",
			     extension);
			rt_extensions_enabled = false;
			return;
		}
	}
	for (const auto* extension: required) {
		AppendExtensionIfMissing(device_extensions, extension);
	}
	if (ExtensionAvailable(available_extensions, VK_KHR_SPIRV_1_4_EXTENSION_NAME)) {
		AppendExtensionIfMissing(device_extensions, VK_KHR_SPIRV_1_4_EXTENSION_NAME);
	}
	if (ExtensionAvailable(available_extensions, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME)) {
		AppendExtensionIfMissing(device_extensions, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
	}

	rt_extensions_enabled = true;
	LOGF("Vulkan RT: enabling hardware ray query extensions\n");
}

void GraphicContext::LoadHardwareRayTracingFunctions() const {
	EXIT_IF(device == nullptr);
	if (!rt_extensions_enabled) {
		return;
	}

	const auto& dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER;
	if (dispatcher.vkGetBufferDeviceAddressKHR == nullptr ||
	    dispatcher.vkCreateAccelerationStructureKHR == nullptr ||
	    dispatcher.vkDestroyAccelerationStructureKHR == nullptr ||
	    dispatcher.vkGetAccelerationStructureBuildSizesKHR == nullptr ||
	    dispatcher.vkCmdBuildAccelerationStructuresKHR == nullptr ||
	    dispatcher.vkGetAccelerationStructureDeviceAddressKHR == nullptr) {
		LOGF("Vulkan RT: required device functions unavailable; hardware ray tracing disabled\n");
		// A driver can advertise the extensions but fail to expose their
		// functions (broken or quirky drivers, notably some iGPU drivers).
		// RT is an optional capability: degrade gracefully instead of killing
		// the process before the user ever loads a game.
		rt_extensions_enabled = false;
		return;
	}
}

} // namespace Libs::Graphics
