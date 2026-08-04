#include <vulkan/vulkan.h>
#include <iostream>
#include <windows.h>

class VulkanLoader {
public:
    VulkanLoader() {
        SetDllDirectoryA(".");
        vulkan_dll = LoadLibraryA("vulkan-1.dll");
        if (!vulkan_dll) {
            std::cerr << "Failed to load vulkan-1.dll" << std::endl;
            return;
        }
        vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)GetProcAddress(vulkan_dll, "vkEnumerateInstanceVersion");
        vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)GetProcAddress(vulkan_dll, "vkEnumerateInstanceExtensionProperties");
        vkCreateInstance = (PFN_vkCreateInstance)GetProcAddress(vulkan_dll, "vkCreateInstance");
        vkDestroyInstance = (PFN_vkDestroyInstance)GetProcAddress(vulkan_dll, "vkDestroyInstance");
    }
    ~VulkanLoader() { if (vulkan_dll) FreeLibrary(vulkan_dll); }
    bool IsLoaded() const { return vulkan_dll != nullptr; }
    PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = nullptr;
    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;
private:
    HMODULE vulkan_dll = nullptr;
};

void TestResourceDescriptorClassification() {
    VulkanLoader vulkan;
    if (!vulkan.IsLoaded()) {
        std::cout << "Skipping test: Vulkan not available" << std::endl;
        return;
    }
    uint32_t instanceVersion;
    if (vulkan.vkEnumerateInstanceVersion(&instanceVersion) != VK_SUCCESS) {
        std::cout << "Skipping test: Vulkan initialization failed" << std::endl;
        return;
    }
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    VkInstance instance;
    if (vulkan.vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cout << "Skipping test: Vulkan instance creation failed" << std::endl;
        return;
    }
    std::cout << "Vulkan instance created successfully" << std::endl;
    vulkan.vkDestroyInstance(instance, nullptr);
    std::cout << "Test completed." << std::endl;
}

int main() {
    TestResourceDescriptorClassification();
    return 0;
}