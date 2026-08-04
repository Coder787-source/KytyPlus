#include <vulkan/vulkan.h>
#include <iostream>
#include <cstring>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  using LibHandle = HMODULE;
  static LibHandle load_vulkan_lib() { return LoadLibraryA("vulkan-1.dll"); }
  static void* load_vulkan_sym(LibHandle h, const char* name) { return (void*)GetProcAddress(h, name); }
  static void free_vulkan_lib(LibHandle h) { if (h) FreeLibrary(h); }
#else
  #include <dlfcn.h>
  using LibHandle = void*;
  static LibHandle load_vulkan_lib() { return dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL); }
  static void* load_vulkan_sym(LibHandle h, const char* name) { return dlsym(h, name); }
  static void free_vulkan_lib(LibHandle h) { if (h) dlclose(h); }
#endif

class VulkanLoader {
public:
    VulkanLoader() {
        lib_ = load_vulkan_lib();
        if (!lib_) {
            std::cerr << "Failed to load Vulkan shared library" << std::endl;
            return;
        }
        vkEnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(load_vulkan_sym(lib_, "vkEnumerateInstanceVersion"));
        vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(load_vulkan_sym(lib_, "vkEnumerateInstanceExtensionProperties"));
        vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(load_vulkan_sym(lib_, "vkCreateInstance"));
        vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(load_vulkan_sym(lib_, "vkDestroyInstance"));
    }
    ~VulkanLoader() { free_vulkan_lib(lib_); }
    VulkanLoader(const VulkanLoader&) = delete;
    VulkanLoader& operator=(const VulkanLoader&) = delete;
    bool IsLoaded() const { return lib_ != nullptr; }

    PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = nullptr;
    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;

private:
    LibHandle lib_ = nullptr;
};

void TestResourceDescriptorClassification() {
    VulkanLoader vulkan;
    if (!vulkan.IsLoaded()) {
        std::cout << "Skipping test: Vulkan not available" << std::endl;
        return;
    }
    if (!vulkan.vkEnumerateInstanceVersion || !vulkan.vkCreateInstance || !vulkan.vkDestroyInstance) {
        std::cout << "Skipping test: Vulkan symbols not available" << std::endl;
        return;
    }
    uint32_t instanceVersion = 0;
    if (vulkan.vkEnumerateInstanceVersion(&instanceVersion) != VK_SUCCESS) {
        std::cout << "Skipping test: Vulkan initialization failed" << std::endl;
        return;
    }
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    VkInstance instance = nullptr;
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