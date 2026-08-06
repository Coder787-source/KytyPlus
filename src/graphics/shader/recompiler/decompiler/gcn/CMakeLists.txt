# SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build the ported shadPS4 GCN (PS4) shader decoder as an isolated static
# library. The gcn/ directory is added as a PRIVATE include root so the ported
# files' relative includes ("common/types.h", "shader_recompiler/frontend/...",
# "core/libraries/kernel/process.h") resolve to the local compatibility shims
# without shadowing Kyty's own src/common headers in the rest of the build.
#
# Scope: only the self-contained decoder core is compiled here. The upstream
# control_flow_graph.{h,cpp} are kept in-tree as a reference but excluded from
# the build because they depend on Boost (small_vector, intrusive/set),
# common/object_pool, and shader_recompiler/ir/condition.h, which Kyty does
# not vendor. Kyty already has its own CFG/IR pipeline (cfg/, ir/) that the
# GcnInst->Decoder::Instruction bridge feeds into, so the upstream CFG is not
# needed for decoding.

add_library(gcn_decoder STATIC
	shader_recompiler/frontend/instruction.cpp
	shader_recompiler/frontend/decode.cpp
	shader_recompiler/frontend/format.cpp
)

target_include_directories(gcn_decoder BEFORE PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

# magic_enum is vendored at the project level; mirror the include root the
# ported files expect (<magic_enum/magic_enum.hpp>).
target_include_directories(gcn_decoder PRIVATE
	${KYTY_THIRD_PARTY_DIR}/magic_enum/include
)

# fmt is a Kyty dependency (via common). The shim headers route brace-style
# formatting through fmt::format, so link fmt in.
target_link_libraries(gcn_decoder PRIVATE fmt::fmt)

# The ported decoder uses brace-style fmt args and modern C++20 features.
set_target_properties(gcn_decoder PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)