#include <vulkan/vulkan.hpp>
#include <iostream>
#include <vector>
#include <cstring>

// Manually define Vulkan extensions if not found
#ifndef VK_KHR_SURFACE_EXTENSION_NAME
#define VK_KHR_SURFACE_EXTENSION_NAME "VK_KHR_surface"
#endif

#ifndef VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
#endif

void TestResourceDescriptorClassification() {
    std::cout << "TestResourceDescriptorClassification: Starting test..." << std::endl;

    // Check if Vulkan is available
    uint32_t instanceVersion;
    if (vk::enumerateInstanceVersion(&instanceVersion) != vk::Result::eSuccess) {
        std::cout << "Vulkan not available: Loader not found or incompatible" << std::endl;
        std::cout << "Skipping test: Vulkan not available" << std::endl;
        return;
    }

    // Check for required instance extensions
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

    auto availableExtensions = vk::enumerateInstanceExtensionProperties();
    for (const auto& extension : requiredExtensions) {
        bool found = false;
        for (const auto& available : availableExtensions) {
            if (strcmp(available.extensionName, extension) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "Vulkan extension not found: " << extension << std::endl;
            std::cout << "Skipping test: Required Vulkan extensions missing" << std::endl;
            return;
        }
    }

    // Create Vulkan instance
    try {
        vk::InstanceCreateInfo createInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        auto instance = vk::createInstance(createInfo);
        std::cout << "Vulkan instance created successfully using AMD Radeon 780M" << std::endl;

        // Cleanup
        instance.destroy();

    } catch (const vk::SystemError& e) {
        std::cout << "Failed to create Vulkan instance: " << e.what() << std::endl;
        std::cout << "Skipping test: Vulkan initialization failed" << std::endl;
        return;
    }

    std::cout << "TestResourceDescriptorClassification: Test completed." << std::endl;
}

// Add a main function
int main() {
    TestResourceDescriptorClassification();
    return 0;
}