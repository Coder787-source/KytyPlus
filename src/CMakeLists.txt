cmake_minimum_required(VERSION 3.12...4.3)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for bundled dependencies")
endif()

project(Kyty)

if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
	set(LINUX TRUE)
endif()

if (NOT (WIN32 OR LINUX OR APPLE))
    message(FATAL_ERROR "only Windows, Linux, and macOS builds are supported")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# CI and the upstream build workflow configure this directory directly with
# `cmake -S src`; the repository-root entry point also adds it as a subdirectory.
# Normalize these paths before loading shared helper functions; utils.cmake uses
# KYTY_SOURCE_DIR when configuring macOS guest address-space support.
if(NOT DEFINED KYTY_SOURCE_DIR OR KYTY_SOURCE_DIR STREQUAL "")
	set(KYTY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(NOT DEFINED KYTY_TESTS_DIR OR KYTY_TESTS_DIR STREQUAL "")
	set(KYTY_TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tests")
endif()

include(utils.cmake)
include(CTest)

option(KYTY_ENABLE_CLANG_TIDY "Run clang-tidy checks during builds" OFF)

set(KYTY_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty")

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include(TestBigEndian)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "only 64-bit builds are supported")
endif()

test_big_endian(BIG_ENDIAN)
if(BIG_ENDIAN)
	set(KYTY_ENDIAN KYTY_ENDIAN_BIG)
else()
	set(KYTY_ENDIAN KYTY_ENDIAN_LITTLE)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(KYTY_BUILD KYTY_BUILD_DEBUG)
else()
	set(KYTY_BUILD KYTY_BUILD_RELEASE)
endif()

if(LINUX OR APPLE)
	# macOS rides the POSIX/Linux code paths until it gets a dedicated platform
	set(KYTY_PLATFORM KYTY_PLATFORM_LINUX)
else()
	set(KYTY_PLATFORM KYTY_PLATFORM_WINDOWS)
endif()

set(CLANG FALSE)
set(GCC FALSE)
set(KYTY_CLANG_CL FALSE)
get_filename_component(KYTY_CXX_COMPILER_NAME "${CMAKE_CXX_COMPILER}" NAME_WE)
string(TOLOWER "${KYTY_CXX_COMPILER_NAME}" KYTY_CXX_COMPILER_NAME)

if(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
	set(CLANG TRUE)
	set(KYTY_COMPILER CLANG)
	if(WIN32 AND KYTY_CXX_COMPILER_NAME STREQUAL "clang-cl")
		set(KYTY_CLANG_CL TRUE)
		set(KYTY_LINKER LLD_LINK)
	elseif(APPLE)
		set(KYTY_LINKER LD64) # Apple's default linker; lld flags don't apply
	else()
		set(KYTY_LINKER LLD)
	endif()
	if ((CMAKE_AR MATCHES "llvm") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0.0) AND (CMAKE_CXX_COMPILER MATCHES "winlibs"))
		string(REPLACE "llvm-ar.exe" "ar.exe" CMAKE_AR ${CMAKE_AR}) # llvm-ar.exe 11.0.0 from winlibs is broken
	endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(GCC TRUE)
	set(KYTY_COMPILER GCC)
	set(KYTY_LINKER LD)
else()
	message(FATAL_ERROR "only Clang and GCC are supported; found ${CMAKE_CXX_COMPILER_ID}")
endif()

set(KYTY_PROJECT_NAME "Emulator" CACHE STRING "Project name suffix")

string(TOLOWER ${KYTY_COMPILER} KYTY_COMPILER_ID)
string(TOLOWER ${KYTY_LINKER} KYTY_LINKER_ID)

if (CLANG AND (KYTY_LINKER STREQUAL LLD))
	set(KYTY_LD_OPTIONS "-fuse-ld=lld")
endif()

if (KYTY_LINKER STREQUAL LD AND NOT LINUX)
	set(KYTY_LD_OPTIONS "-Wl,--image-base=0x100000000000")
endif()

project(Kyty${KYTY_PROJECT_NAME}${CMAKE_BUILD_TYPE}${KYTY_COMPILER} VERSION 0.2.2)

include_directories(	
	${KYTY_THIRD_PARTY_DIR}/gtest/include
	${KYTY_THIRD_PARTY_DIR}/gtest
	${KYTY_THIRD_PARTY_DIR}/fmt/include
	${KYTY_THIRD_PARTY_DIR}/magic_enum/include
	${PROJECT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
)

set(KYTY_VERSION "${PROJECT_VERSION}")
# cmake_config.h.in lives next to this CMakeLists.txt; point configure_file at it
# directly so the merged tree works without a duplicate top-level registration.
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake_config.h.in
  ${PROJECT_BINARY_DIR}/cmake_config.h
  )

find_package(Git)

# KytyGitVersion — proper file-level dependency, not a phantom target.
# Output goes to ${PROJECT_BINARY_DIR}/kytyGitVersion.h (NOT under src/) so it lands on the
# include path ("-I${PROJECT_BINARY_DIR}") that the common library's compile flags add.
# The custom_command declares kytyGitVersion.h as OUTPUT so ninja guarantees the file
# exists before the dependents (common, kyty_emulator) compile.
add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/kytyGitVersion.h
    COMMAND ${CMAKE_COMMAND}
        -D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/kytyGitVersion.h.in
        -D OUTPUT_FILE=${PROJECT_BINARY_DIR}/kytyGitVersion.h
        -D GIT_EXECUTABLE=${GIT_EXECUTABLE}
        -D GIT_WORKING_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/generate_version.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kytyGitVersion.h.in
    COMMENT "Generate kytyGitVersion.h"
    VERBATIM
)
add_custom_target(KytyGitVersion DEPENDS ${PROJECT_BINARY_DIR}/kytyGitVersion.h)

# When src/ is configured directly (the CI path), the root CMakeLists.txt has
# not created this interface target. Create it here after KytyGitVersion exists;
# the root entry point reuses its existing target instead.
if(NOT TARGET kyty_git_version)
	add_library(kyty_git_version INTERFACE)
endif()
add_dependencies(kyty_git_version KytyGitVersion)

if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0.0)
	list(APPEND KYTY_IWYU 
		kyty_emulator
		common
		#launcher
	)
	list(APPEND KYTY_CLANG_TIDY
		kyty_emulator
		#common
		launcher
	)
endif()

option(KYTY_BUILD_LAUNCHER "Build Qt launcher" ON)
  
config_compiler_and_linker()  
  
add_subdirectory("${KYTY_THIRD_PARTY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/3rdparty")
add_subdirectory(common)
add_subdirectory(package)

add_subdirectory(graphics/shader/recompiler/decompiler/gcn)

file(GLOB kyty_emulator_src CONFIGURE_DEPENDS
	libs/*.cpp
	libs/*.h
	graphics/*.cpp
	graphics/*.h
	graphics/guest_gpu/*.cpp
	graphics/guest_gpu/*.h
	graphics/guest_gpu/command_processor/*.cpp
	graphics/guest_gpu/command_processor/*.h
	graphics/host_gpu/*.cpp
	graphics/host_gpu/*.h
	graphics/host_gpu/renderer/*.cpp
	graphics/host_gpu/renderer/*.h
	graphics/host_gpu/renderer/cache/*.cpp
	graphics/host_gpu/renderer/cache/*.h
	graphics/host_gpu/renderer/image/*.cpp
	graphics/host_gpu/renderer/image/*.h
	graphics/host_gpu/renderer/pipeline/*.cpp
	graphics/host_gpu/renderer/pipeline/*.h
	graphics/rt/*.cpp
	graphics/rt/*.h
	graphics/shader/*.cpp
	graphics/shader/*.h
	graphics/shader/recompiler/*.cpp
	graphics/shader/recompiler/*.h
	graphics/shader/recompiler/cfg/*.cpp
	graphics/shader/recompiler/cfg/*.h
	graphics/shader/recompiler/decompiler/*.cpp
	graphics/shader/recompiler/decompiler/*.h
	graphics/shader/recompiler/emitter/*.cpp
	graphics/shader/recompiler/emitter/*.h
	graphics/shader/recompiler/ir/*.cpp
	graphics/shader/recompiler/ir/*.h
	graphics/presentation/*.cpp
	graphics/presentation/*.h
	graphics/presentation/window/*.cpp
	graphics/presentation/window/*.h
	kernel/*.cpp
	kernel/*.h
	loader/*.cpp
	loader/*.h
)

find_program(KYTY_GLSLANG_VALIDATOR glslangValidator REQUIRED)
set(gpu_tiler_shader_dir "${CMAKE_CURRENT_SOURCE_DIR}/graphics/host_gpu/shaders")
set(gpu_tiler_generated_dir "${PROJECT_BINARY_DIR}/gpu_tiler_shaders")
set(gpu_tiler_shader_names
	standard256
	standard4
	standard4_3d
	standard64
	standard64_3d
	prt
	prt_3d
	render_target
	depth
	promote_d16
	demote_d16
	swap_bgra16
)
file(GLOB gpu_tiler_shader_includes CONFIGURE_DEPENDS "${gpu_tiler_shader_dir}/gpu_tiler_*.inc")
foreach(shader_name IN LISTS gpu_tiler_shader_names)
	set(shader_source "${gpu_tiler_shader_dir}/gpu_tiler_${shader_name}.comp")
	set(shader_spv "${gpu_tiler_generated_dir}/gpu_tiler_${shader_name}.spv")
	set(shader_header "${gpu_tiler_generated_dir}/gpu_tiler_${shader_name}_spv.h")
	string(TOUPPER "GPU_TILER_${shader_name}_SPV" shader_symbol)
	add_custom_command(
		OUTPUT "${shader_header}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${gpu_tiler_generated_dir}"
		COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0 -Os
		        "-I${gpu_tiler_shader_dir}" -o "${shader_spv}" "${shader_source}"
		COMMAND ${CMAKE_COMMAND} -DINPUT=${shader_spv} -DOUTPUT=${shader_header}
		        -DSYMBOL=${shader_symbol} -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
		DEPENDS "${shader_source}" ${gpu_tiler_shader_includes}
		VERBATIM
	)
	list(APPEND gpu_tiler_shader_headers "${shader_header}")
endforeach()
list(APPEND kyty_emulator_src ${gpu_tiler_shader_headers})

set(gpu_blit_generated_dir "${PROJECT_BINARY_DIR}/gpu_blit_shaders")
set(gpu_blit_shader_sources
	"${gpu_tiler_shader_dir}/gpu_blit_fs_triangle.vert"
	"${gpu_tiler_shader_dir}/gpu_blit_color_to_ms_depth.frag"
)
foreach(shader_source IN LISTS gpu_blit_shader_sources)
	get_filename_component(shader_name "${shader_source}" NAME_WE)
	set(shader_spv "${gpu_blit_generated_dir}/${shader_name}.spv")
	set(shader_header "${gpu_blit_generated_dir}/${shader_name}_spv.h")
	string(TOUPPER "${shader_name}_SPV" shader_symbol)
	add_custom_command(
		OUTPUT "${shader_header}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${gpu_blit_generated_dir}"
		COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0 -Os
		        "-I${gpu_tiler_shader_dir}" -o "${shader_spv}" "${shader_source}"
		COMMAND ${CMAKE_COMMAND} -DINPUT=${shader_spv} -DOUTPUT=${shader_header}
		        -DSYMBOL=${shader_symbol} -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
		DEPENDS "${shader_source}"
		VERBATIM
	)
	list(APPEND gpu_blit_shader_headers "${shader_header}")
endforeach()
list(APPEND kyty_emulator_src ${gpu_blit_shader_headers})

# FSR upscaler shaders (EASU spatial upscale + RCAS sharpening).
set(fsr_generated_dir "${PROJECT_BINARY_DIR}/fsr_shaders")
set(fsr_shader_sources
	"${gpu_tiler_shader_dir}/fsr_easu.comp"
	"${gpu_tiler_shader_dir}/fsr_rcas.comp"
)
foreach(shader_source IN LISTS fsr_shader_sources)
	get_filename_component(shader_name "${shader_source}" NAME_WE)
	set(shader_spv "${fsr_generated_dir}/${shader_name}.spv")
	set(shader_header "${fsr_generated_dir}/${shader_name}_spv.h")
	string(TOUPPER "${shader_name}_SPV" shader_symbol)
	add_custom_command(
		OUTPUT "${shader_header}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${fsr_generated_dir}"
		COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0 -Os
		        "-I${gpu_tiler_shader_dir}" -o "${shader_spv}" "${shader_source}"
		COMMAND ${CMAKE_COMMAND} -DINPUT=${shader_spv} -DOUTPUT=${shader_header}
		        -DSYMBOL=${shader_symbol} -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
		DEPENDS "${shader_source}"
		VERBATIM
	)
	list(APPEND fsr_shader_headers "${shader_header}")
endforeach()

# Generate the FSR headers at configure time as well, so they already exist on disk
# before ninja schedules any kyty_emulator translation unit. fsrUpscaler.cpp is collected
# via file(GLOB) and its #include of the generated header can otherwise be compiled before
# the add_custom_command OUTPUT is produced (a build-order race that the order-only
# dependency on the fsr_shaders custom target does not always win under parallel load).
foreach(shader_source IN LISTS fsr_shader_sources)
	get_filename_component(shader_name "${shader_source}" NAME_WE)
	set(shader_spv "${fsr_generated_dir}/${shader_name}.spv")
	set(shader_header "${fsr_generated_dir}/${shader_name}_spv.h")
	string(TOUPPER "${shader_name}_SPV" shader_symbol)
	execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${fsr_generated_dir}"
		RESULT_VARIABLE fsr_mkdir_result)
	if(NOT fsr_mkdir_result EQUAL 0)
		message(FATAL_ERROR "Failed to create FSR shader output dir ${fsr_generated_dir}")
	endif()
	execute_process(
		COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0 -Os
		        "-I${gpu_tiler_shader_dir}" -o "${shader_spv}" "${shader_source}"
		RESULT_VARIABLE fsr_compile_result
		OUTPUT_VARIABLE fsr_compile_out
		ERROR_VARIABLE fsr_compile_err)
	if(NOT fsr_compile_result EQUAL 0)
		message(FATAL_ERROR "glslangValidator failed for ${shader_source}\n${fsr_compile_out}\n${fsr_compile_err}")
	endif()
	execute_process(
		COMMAND ${CMAKE_COMMAND} -DINPUT=${shader_spv} -DOUTPUT=${shader_header}
		        -DSYMBOL=${shader_symbol} -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
		RESULT_VARIABLE fsr_embed_result)
	if(NOT fsr_embed_result EQUAL 0)
		message(FATAL_ERROR "embed_spirv.cmake failed for ${shader_spv} -> ${shader_header}")
	endif()
endforeach()
list(APPEND kyty_emulator_src ${fsr_shader_headers})

# fsrUpscaler.cpp #includes the generated fsr_*_spv.h headers. They are generated at
# configure time above, so they already exist before any kyty_emulator source compiles.
# Keep the custom target too so a clean rebuild re-runs the per-file generation.
add_custom_target(fsr_shaders DEPENDS ${fsr_shader_headers})

list(APPEND kyty_emulator_src
	emulator.h
	emulator.cpp
)
list(REMOVE_DUPLICATES kyty_emulator_src)

if (KYTY_CLANG_CL)
	set_source_files_properties(${kyty_emulator_src} PROPERTIES COMPILE_FLAGS "-Wno-pragma-pack -Wno-deprecated-declarations -D_TIMESPEC_DEFINED")
endif()

set(kyty_emulator_link_libraries common Vulkan::Headers spirv-tools-opt spirv-tools SDL2-static imgui xxhash FFmpeg::ffmpeg fmt::fmt nlohmann_json::nlohmann_json LibAtrac9 kyty_git_version Zydis::Zydis xbyak::xbyak)

# zlib is required by rpfArchiveReader on all platforms.
find_package(ZLIB)
if(ZLIB_FOUND)
	list(APPEND kyty_emulator_link_libraries ZLIB::ZLIB)
	list(APPEND kyty_emulator_compile_definitions KYTY_HAS_ZLIB=1)
endif()

# Linux system libraries required by the static FFmpeg archive.
if(LINUX)
	find_package(Threads REQUIRED)
	list(APPEND kyty_emulator_link_libraries m ${CMAKE_DL_LIBS} Threads::Threads)

	# Optional FFmpeg dependencies.
	find_library(KYTY_BZ2_LIBRARY bz2)
	if(KYTY_BZ2_LIBRARY)
		list(APPEND kyty_emulator_link_libraries ${KYTY_BZ2_LIBRARY})
	endif()
	find_library(KYTY_LZMA_LIBRARY lzma)
	if(KYTY_LZMA_LIBRARY)
		list(APPEND kyty_emulator_link_libraries ${KYTY_LZMA_LIBRARY})
	endif()
endif()

set(inc_headers
	${CMAKE_CURRENT_SOURCE_DIR}
	${KYTY_THIRD_PARTY_DIR}/SDL2/include
	${KYTY_THIRD_PARTY_DIR}/VulkanMemoryAllocator/include
	${KYTY_THIRD_PARTY_DIR}/SPIRV-Tools/include
	${KYTY_THIRD_PARTY_DIR}/fmt/include
	${KYTY_THIRD_PARTY_DIR}/xxhash
	${KYTY_THIRD_PARTY_DIR}/cpuinfo/include
	${KYTY_THIRD_PARTY_DIR}/stb
)

if (KYTY_CLANG_CL)
	list(APPEND kyty_emulator_link_libraries winpthread)
	list(APPEND inc_headers 
		${KYTY_THIRD_PARTY_DIR}/winpthread/include
	)
endif()

list(APPEND check_headers
	${CMAKE_CURRENT_SOURCE_DIR}
)

# add_kyty_full_emulator_test is declared in src/utils.cmake (one canonical
# definition for the merged tree — previously duplicated here and at top-level).

add_kyty_full_emulator_test(shader_cfg_tests ../tests/shaderCfgTests.cpp)

add_executable(scalar_provenance_tests EXCLUDE_FROM_ALL
	../tests/ScalarProvenanceTests.cpp
	graphics/host_gpu/hostMemory.cpp
	graphics/shader/recompiler/ir/ScalarProvenance.cpp
	graphics/shader/recompiler/ir/SrtWalker.cpp
)
target_link_libraries(scalar_provenance_tests fmt::fmt)
target_include_directories(scalar_provenance_tests PRIVATE ${inc_headers})

add_executable(page_manager_tests EXCLUDE_FROM_ALL
	../tests/PageManagerTests.cpp
	graphics/host_gpu/pageManager.cpp
)
target_include_directories(page_manager_tests PRIVATE ${inc_headers})

add_executable(memory_tracker_tests EXCLUDE_FROM_ALL
	../tests/MemoryTrackerTests.cpp
	graphics/host_gpu/pageManager.cpp
	graphics/host_gpu/memoryTracker.cpp
)
target_link_libraries(memory_tracker_tests fmt::fmt common)
target_include_directories(memory_tracker_tests PRIVATE ${inc_headers})
target_compile_definitions(memory_tracker_tests PRIVATE KYTY_MEMORY_TRACKER_TESTS=1)

add_executable(shader_vertex_metadata_tests EXCLUDE_FROM_ALL
	../tests/ShaderVertexMetadataTests.cpp
	graphics/host_gpu/hostMemory.cpp
	graphics/shader/shaderVertexMetadata.cpp
)
target_include_directories(shader_vertex_metadata_tests PRIVATE ${inc_headers})

add_executable(pipeline_cache_data_tests EXCLUDE_FROM_ALL
	../tests/PipelineCacheDataTests.cpp
	graphics/host_gpu/renderer/pipelineCacheData.cpp
)
target_link_libraries(pipeline_cache_data_tests Vulkan::Headers)
target_include_directories(pipeline_cache_data_tests PRIVATE ${inc_headers})

add_executable(graphics_audio_semantics_tests EXCLUDE_FROM_ALL
	../tests/GraphicsAudioSemanticsTests.cpp
	loader/x64InstructionEmulator.cpp
	loader/nullPageFaultLog.cpp
)
target_link_libraries(graphics_audio_semantics_tests Vulkan::Headers common fmt::fmt)
target_include_directories(graphics_audio_semantics_tests PRIVATE ${inc_headers})

add_kyty_full_emulator_test(pm4_ngs2_fuzz_tests ../tests/Pm4Ngs2FuzzTests.cpp)

add_kyty_full_emulator_test(soft_ignore_evidence_tests ../tests/SoftIgnoreEvidenceTests.cpp)

add_executable(shader_stage_runtime_tests EXCLUDE_FROM_ALL
	../tests/ShaderStageRuntimeTests.cpp
	graphics/guest_gpu/gpu_format.cpp
	graphics/host_gpu/hostMemory.cpp
	graphics/shader/shaderStageRuntime.cpp
	graphics/shader/recompiler/ir/ResourceMaterialization.cpp
	graphics/shader/recompiler/ir/ScalarProvenance.cpp
	graphics/shader/recompiler/ir/SrtWalker.cpp
)
target_link_libraries(shader_stage_runtime_tests fmt::fmt)
target_include_directories(shader_stage_runtime_tests PRIVATE ${inc_headers})

add_executable(resource_tracking_tests EXCLUDE_FROM_ALL
	../tests/ResourceTrackingTests.cpp
	graphics/guest_gpu/gpu_format.cpp
	graphics/host_gpu/hostMemory.cpp
	graphics/shader/recompiler/ir/ScalarProvenance.cpp
	graphics/shader/recompiler/ir/SrtWalker.cpp
	graphics/shader/recompiler/ir/SrtPatcher.cpp
	graphics/shader/recompiler/ir/ResourceTracking.cpp
	graphics/shader/recompiler/ir/ResourceMaterialization.cpp
	graphics/shader/recompiler/ir/ShaderInfoCollection.cpp
	graphics/shader/recompiler/ir/BindingLayout.cpp
)
target_link_libraries(resource_tracking_tests fmt::fmt)
target_include_directories(resource_tracking_tests PRIVATE ${inc_headers})

add_executable(resource_mutex_tests EXCLUDE_FROM_ALL
	../tests/ResourceMutexTests.cpp
	graphics/host_gpu/renderer/cache/resourceMutex.cpp
)
target_link_libraries(resource_mutex_tests common)
target_include_directories(resource_mutex_tests PRIVATE ${inc_headers})

add_executable(event_queue_lifetime_tests EXCLUDE_FROM_ALL
	../tests/EventQueueLifetimeTests.cpp
	kernel/eventQueue.cpp
	loader/timer.cpp
)
target_link_libraries(event_queue_lifetime_tests common fmt::fmt)
target_include_directories(event_queue_lifetime_tests PRIVATE ${inc_headers})

add_executable(image_page_table_tests EXCLUDE_FROM_ALL
	../tests/ImagePageTableTests.cpp
)
target_link_libraries(image_page_table_tests common fmt::fmt)
target_include_directories(image_page_table_tests PRIVATE ${inc_headers})

add_kyty_full_emulator_test(shader_recompiler_compute_tests ../tests/ShaderRecompilerComputeTests.cpp)

set(gpu_test_generated_dir "${PROJECT_BINARY_DIR}/gpu_test_shaders")
set(gpu_test_ms_depth_source
	"${gpu_tiler_shader_dir}/gpu_test_ms_depth.comp")
set(gpu_test_ms_depth_spv
	"${gpu_test_generated_dir}/gpu_test_ms_depth.spv")
set(gpu_test_ms_depth_header
	"${gpu_test_generated_dir}/gpu_test_ms_depth_spv.h")
add_custom_command(
	OUTPUT "${gpu_test_ms_depth_header}"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${gpu_test_generated_dir}"
	COMMAND "${KYTY_GLSLANG_VALIDATOR}" -V --target-env vulkan1.0 -Os
	        -o "${gpu_test_ms_depth_spv}" "${gpu_test_ms_depth_source}"
	COMMAND ${CMAKE_COMMAND} -DINPUT=${gpu_test_ms_depth_spv}
	        -DOUTPUT=${gpu_test_ms_depth_header} -DSYMBOL=GPU_TEST_MS_DEPTH_SPV
	        -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
	DEPENDS "${gpu_test_ms_depth_source}"
	VERBATIM
)
target_sources(shader_recompiler_compute_tests PRIVATE
	"${gpu_test_ms_depth_header}")

add_kyty_full_emulator_test(virtual_memory_allocation_tests ../tests/VirtualMemoryAllocationTests.cpp)
target_compile_definitions(virtual_memory_allocation_tests PRIVATE
	KYTY_VIRTUAL_MEMORY_ALLOCATION_TESTS=1)

# These tests use exceptions.
if(NOT KYTY_CLANG_CL)
	foreach(kyty_exception_test scalar_provenance_tests resource_tracking_tests
			virtual_memory_allocation_tests)
		target_compile_options(${kyty_exception_test} PRIVATE -fexceptions)
	endforeach()
endif()

if(BUILD_TESTING)
	add_test(NAME image_page_table COMMAND $<TARGET_FILE:image_page_table_tests>)
	add_test(NAME memory_tracker COMMAND $<TARGET_FILE:memory_tracker_tests>)
	add_test(NAME page_manager COMMAND $<TARGET_FILE:page_manager_tests>)
	add_test(NAME pipeline_cache_data COMMAND $<TARGET_FILE:pipeline_cache_data_tests>)
	add_test(NAME graphics_audio_semantics COMMAND $<TARGET_FILE:graphics_audio_semantics_tests>)
	add_test(NAME pm4_ngs2_fuzz COMMAND $<TARGET_FILE:pm4_ngs2_fuzz_tests> --iterations=4096)
	add_test(NAME soft_ignore_evidence COMMAND $<TARGET_FILE:soft_ignore_evidence_tests>)
	add_test(NAME resource_mutex COMMAND $<TARGET_FILE:resource_mutex_tests>)
	add_test(NAME event_queue_lifetime COMMAND $<TARGET_FILE:event_queue_lifetime_tests>)
	add_test(NAME shader_recompiler_compute COMMAND $<TARGET_FILE:shader_recompiler_compute_tests>)
	add_test(NAME command_scheduler_timeline
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --scheduler-only)
	add_test(NAME stream_buffer_ring
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --stream-buffer-only)
	add_test(NAME gpu_command_lane
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --gpu-command-lane-only)
	add_test(NAME gpu_tiler
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --gpu-tiler-only)

	add_test(NAME texture_cache_layered_image
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --layered-image-only)
	add_test(NAME texture_cache_image_views
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --image-view-cache-only)
	add_test(NAME texture_cache_storage_sampled
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --storage-sampled-only)
	add_test(NAME texture_cache_depth_readback
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --depth-readback-only)
	add_test(NAME buffer_cache_dirty_gc
		COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --buffer-cache-gc-only)

	if(WIN32)
		# These tests still depend on the Windows multisample-depth path.
		add_test(NAME texture_cache_image_overlap
			COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --image-overlap-only)
		add_test(NAME texture_cache_htile_clear
			COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --htile-clear-only)
		add_test(NAME buffer_cache_ranges
			COMMAND $<TARGET_FILE:shader_recompiler_compute_tests> --buffer-cache-range-only)
	endif()
endif()


add_executable(kyty_emulator main.cpp platformDispatch.cpp shadps4Embedder.cpp ${kyty_emulator_src})
# Build the generated FSR shader headers before any kyty_emulator source compiles
# (fsrUpscaler.cpp #includes them and is GLOB-collected, so it needs an explicit edge).
add_dependencies(kyty_emulator fsr_shaders)

# Anchorx86_64 macOS-guest .zerofill segments; selected by configure_macos_guest_address_space.
configure_macos_guest_address_space(kyty_emulator)

target_link_libraries(kyty_emulator ${kyty_emulator_link_libraries} gcn_decoder package)
target_compile_definitions(kyty_emulator PRIVATE ${kyty_emulator_compile_definitions})
if (WIN32)
	target_link_libraries(kyty_emulator opengl32)
	target_link_libraries(kyty_emulator iphlpapi)
	target_link_libraries(kyty_emulator ws2_32)
endif()
if (KYTY_CLANG_CL)
	target_link_libraries(kyty_emulator onecore)
endif()
if (CLANG AND NOT KYTY_CLANG_CL)
	target_link_libraries(kyty_emulator pthread)
endif()
# dlopen/dlsym/dladdr for RenderDoc.
target_link_libraries(kyty_emulator ${CMAKE_DL_LIBS})
target_include_directories(kyty_emulator PRIVATE ${inc_headers})
# GcnBridge.cpp bridges into the ported shadPS4 decoder; give it the gcn/
# include root with precedence over src/ so the ported headers resolve to
# the local compatibility shims (common/types.h, common/assert.h, ...) and
# not to Kyty's own src/common headers, which define different macros.
set_source_files_properties(graphics/shader/recompiler/decompiler/GcnBridge.cpp
	PROPERTIES COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/graphics/shader/recompiler/decompiler/gcn")

clang_tidy_check(kyty_emulator "" "${check_headers}" "${inc_headers}")

include_what_you_use(kyty_emulator "${inc_headers}")

set(KYTY_EMULATOR_MAP_NAME "kyty_emulator_${KYTY_COMPILER_ID}_${KYTY_LINKER_ID}.map")

set(KYTY_EMULATOR_MAP_LINK_PATH "${CMAKE_CURRENT_BINARY_DIR}/${KYTY_EMULATOR_MAP_NAME}")
set(KYTY_EMULATOR_PDB_LINK_PATH "${CMAKE_CURRENT_BINARY_DIR}/kyty_emulator.pdb")

if(KYTY_CLANG_CL)
	set_target_properties(kyty_emulator PROPERTIES LINK_FLAGS "/DYNAMICBASE:NO /DEBUG:FULL /PDB:${KYTY_EMULATOR_PDB_LINK_PATH} /lldmap:${KYTY_EMULATOR_MAP_LINK_PATH}")
	add_custom_command(TARGET kyty_emulator POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KYTY_THIRD_PARTY_DIR}/winpthread/bin/libwinpthread-1.dll" $<TARGET_FILE_DIR:kyty_emulator>/libwinpthread-1.dll)
elseif(WIN32 OR LINUX)
	set_target_properties(kyty_emulator PROPERTIES LINK_FLAGS "${KYTY_LD_OPTIONS} -Wl,-Map=${KYTY_EMULATOR_MAP_LINK_PATH}")
endif()

add_dependencies(kyty_emulator KytyGitVersion)

if(APPLE)
	# The emulator writes x86-64 trampolines/PLT stubs into memory and executes them, and
	# runs guest code under Rosetta. That requires the JIT / unsigned-executable-memory
	# entitlements, so re-sign the binary after every link (an unsigned/relinked binary
	# reverts to the hardened defaults and aborts when it first executes written code).
	add_custom_command(TARGET kyty_emulator POST_BUILD
		COMMAND codesign -s - --force --options runtime
			--entitlements "${CMAKE_CURRENT_SOURCE_DIR}/macos_jit.entitlements"
			$<TARGET_FILE:kyty_emulator>
		COMMENT "Codesign kyty_emulator with JIT entitlements (macOS)")
endif()

install(TARGETS kyty_emulator DESTINATION .)
# Bundle a real shadPS4 runtime for PS4 game delegation (opt-in).
# Build with -DKYTY_BUNDLE_SHADPS4=ON to produce a sibling shadps4 binary
# that the dispatcher auto-discovers; see platformDispatch.cpp.
include(${CMAKE_CURRENT_SOURCE_DIR}/shadps4_bundle.cmake)
if(KYTY_BUILD_LAUNCHER)
	add_subdirectory(launcher)
endif()
if(KYTY_CLANG_CL)
	install(FILES "${KYTY_THIRD_PARTY_DIR}/winpthread/bin/libwinpthread-1.dll" DESTINATION .)
endif()
