# SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# shadPS4 superbuild module.
#
# Bundles a real shadPS4 runtime into the Kyty build so that a PS4 title is
# executed by the genuine, community-tested shadPS4 stack instead of a
# re-implementation. See ../platformDispatch.cpp for the dispatch logic and
# the auto-discovery of the sibling shadps4 binary.
#
# Why ExternalProject_Add (isolated build) and NOT add_subdirectory:
#   shadPS4 and Kyty are mutually incompatible as a single CMake tree:
#     - shadPS4 requires C++23; Kyty uses C++20.
#     - shadPS4 links SDL3; Kyty links SDL2-static.
#     - shadPS4 fetches ~25 externals (Boost, xbyak, Zydis, glslang, sirit,
#       ZArchive, toml11, OpenAL, Freetype, protobuf, ...) via FetchContent
#       that Kyty does not vendor.
#     - Both define LOG_INFO / Common::Singleton / ASSERT / Common::FS with
#       different signatures -> link-time symbol collisions.
#   ExternalProject_Add builds shadPS4 in its OWN configure+build tree with
#   its own C++23, SDL3, and externals, so none of those collide with Kyty.
#   The only thing that crosses the boundary is the produced binary, which
#   we copy next to kyty_emulator.exe and let the dispatcher auto-discover.
#
# Enabled with: cmake -DKYTY_BUNDLE_SHADPS4=ON ...
# Disabled by default (it adds a substantial isolated build).
#
# Network access is required on first configure to clone shadPS4 and its
# externals; subsequent builds reuse the source/binary stamps.

option(KYTY_BUNDLE_SHADPS4 "Build and bundle a real shadPS4 runtime for PS4 game delegation (adds an isolated C++23/SDL3 build)" OFF)

if(NOT KYTY_BUNDLE_SHADPS4)
	return()
endif()

include(ExternalProject)

# Default to the upstream main branch; override with -DKYTY_SHADPS4_GIT_TAG=<tag/sha>.
set(KYTY_SHADPS4_GIT_URL "https://github.com/shadps4-emu/shadPS4.git" CACHE STRING "shadPS4 git URL")
set(KYTY_SHADPS4_GIT_TAG "main" CACHE STRING "shadPS4 git tag/branch/sha")

# Isolated prefix so shadPS4's source, build, and install trees never touch
# Kyty's CMake state or its 3rdparty/ submodules.
set(SHADPS4_PREFIX "${CMAKE_BINARY_DIR}/shadps4_bundle")
set(SHADPS4_SRC_DIR "${SHADPS4_PREFIX}/src")
set(SHADPS4_BUILD_DIR "${SHADPS4_PREFIX}/build")
set(SHADPS4_INSTALL_DIR "${SHADPS4_PREFIX}/install")

# The binary name shadPS4 produces (target `shadps4` -> shadps4.exe / shadps4).
if(WIN32)
	set(SHADPS4_BIN_NAME "shadps4.exe")
else()
	set(SHADPS4_BIN_NAME "shadps4")
endif()
set(SHADPS4_INSTALLED_BIN "${SHADPS4_INSTALL_DIR}/bin/${SHADPS4_BIN_NAME}")

# Generator: inherit the same as the outer project where possible. shadPS4
# supports Ninja, Make, and Visual Studio; we pass the configured generator
# through so a single toolchain is used.
set(SHADPS4_CMAKE_GENERATOR "")
if(CMAKE_GENERATOR)
	set(SHADPS4_CMAKE_GENERATOR "-G${CMAKE_GENERATOR}")
endif()

# Forward a sensible build type. shadPS4 defaults to Release; honor Kyty's.
set(SHADPS4_BUILD_TYPE "Release")
if(CMAKE_BUILD_TYPE)
	set(SHADPS4_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
endif()

# Toolchain: forward the compilers so the isolated build uses the same ones.
set(SHADPS4_CMAKE_ARGS
	-DCMAKE_BUILD_TYPE=${SHADPS4_BUILD_TYPE}
	-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
	-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
	-DCMAKE_INSTALL_PREFIX=${SHADPS4_INSTALL_DIR}
	# Disable features Kyty does not need from the bundled shadPS4 to trim the
	# build: no auto-updater (no release signing), no tests, no Discord RPC.
	-DENABLE_UPDATER=OFF
	-DENABLE_DISCORD_RPC=OFF
	-DENABLE_TESTS=OFF
	# Use shadPS4's vendored externals (FetchContent), not system libs, so the
	# isolated build is self-contained regardless of the host's packages.
	-DENABLE_SYSTEM_LIBRARIES=OFF
)

ExternalProject_Add(shadps4_bundle
	PREFIX ${SHADPS4_PREFIX}
	GIT_REPOSITORY ${KYTY_SHADPS4_GIT_URL}
	GIT_TAG ${KYTY_SHADPS4_GIT_TAG}
	GIT_SHALLOW ON
	SOURCE_DIR ${SHADPS4_SRC_DIR}
	BINARY_DIR ${SHADPS4_BUILD_DIR}
	CMAKE_GENERATOR "${CMAKE_GENERATOR}"
	CMAKE_ARGS ${SHADPS4_CMAKE_ARGS}
	BUILD_BYPRODUCTS ${SHADPS4_INSTALLED_BIN}
	# shadPS4's CMakeLists produces an executable, not an install rule by
	# default; use a custom build step to stage the binary into our install
	# dir so the post-build copy has a stable source path.
	BUILD_COMMAND ${CMAKE_COMMAND} --build . --config ${SHADPS4_BUILD_TYPE} --target shadps4
	INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADPS4_INSTALL_DIR}/bin
)

# Where the staged shadps4 binary actually lands after the build. shadPS4's
# executable target `shadps4` emits to its binary dir (release subdir under
# VS). This glob is resolved at build time by the custom command below, not
# at configure time, so multi-config generators work.
if(WIN32 AND CMAKE_GENERATOR MATCHES "Visual Studio")
	set(SHADPS4_BUILD_OUT_GLOB "${SHADPS4_BUILD_DIR}/${SHADPS4_BUILD_TYPE}/${SHADPS4_BIN_NAME}")
else()
	set(SHADPS4_BUILD_OUT_GLOB "${SHADPS4_BUILD_DIR}/${SHADPS4_BIN_NAME}")
endif()

# Stage the built binary into a stable install path.
add_custom_command(TARGET shadps4_bundle POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADPS4_INSTALL_DIR}/bin
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	        ${SHADPS4_BUILD_OUT_GLOB} ${SHADPS4_INSTALLED_BIN}
	COMMENT "Staging bundled shadPS4 binary")

# Copy the staged shadps4 binary next to kyty_emulator so the dispatcher
# auto-discovers it (FindShadps4Binary checks the sibling dir first). This
# runs as a post-build step on kyty_emulator and depends on the bundle, so
# `make kyty_emulator` pulls in the bundled shadps4 automatically.
if(TARGET kyty_emulator)
	add_dependencies(kyty_emulator shadps4_bundle)
	add_custom_command(TARGET kyty_emulator POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		        ${SHADPS4_INSTALLED_BIN}
		        $<TARGET_FILE_DIR:kyty_emulator>/${SHADPS4_BIN_NAME}
		COMMENT "Copying bundled shadPS4 next to kyty_emulator")
	# Also install the bundled binary alongside kyty_emulator so a packaged
	# build ships both.
	install(FILES ${SHADPS4_INSTALLED_BIN} DESTINATION .)
endif()

message(STATUS "KYTY_BUNDLE_SHADPS4=ON: shadPS4 will be built isolated and bundled (tag=${KYTY_SHADPS4_GIT_TAG}). First configure needs network access.")
