cmake_minimum_required(VERSION 3.20)
project(shader_cfg_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(shader_cfg_tests shaderCfgTests.cpp)

# Vulkan SDK fallback logic
find_package(Vulkan QUIET)
if (Vulkan_FOUND)
    target_link_libraries(shader_cfg_tests PRIVATE Vulkan::Vulkan)
else()
    if (WIN32)
        if (DEFINED ENV{VULKAN_SDK})
            target_link_libraries(shader_cfg_tests PRIVATE "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib")
        else()
            find_library(VULKAN_LIB vulkan-1 PATHS "C:/Windows/System32" "C:/VulkanSDK" "C:/Program Files/VulkanSDK")
            if (VULKAN_LIB)
                target_link_libraries(shader_cfg_tests PRIVATE ${VULKAN_LIB})
            else()
                message(WARNING "vulkan-1.lib not found. Using dynamic loading.")
            endif()
        endif()
    elseif(UNIX AND NOT APPLE)
        # Linux: dynamic loading via dlopen
        target_link_libraries(shader_cfg_tests PRIVATE dl)
    elseif(APPLE)
        # macOS: Vulkan via MoltenVK/loader
        find_library(VULKAN_LIB vulkan)
        if (VULKAN_LIB)
            target_link_libraries(shader_cfg_tests PRIVATE ${VULKAN_LIB})
        else()
            target_link_libraries(shader_cfg_tests PRIVATE dl)
        endif()
    endif()
endif()

# Force console subsystem on Windows
if (WIN32)
    target_link_options(shader_cfg_tests PRIVATE "LINKER:/SUBSYSTEM:CONSOLE")
endif()

set(CMAKE_VERBOSE_MAKEFILE ON)