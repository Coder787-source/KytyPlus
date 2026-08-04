find_package(Vulkan REQUIRED)

add_executable(shader_cfg_tests
    shaderCfgTests.cpp
    ${SHADER_CFG_TEST_SOURCES}
)

target_link_libraries(shader_cfg_tests PRIVATE Vulkan::Vulkan)