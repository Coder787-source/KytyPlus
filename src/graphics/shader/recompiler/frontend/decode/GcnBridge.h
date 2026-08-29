// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Bridge: drives the ported shadPS4 GCN decoder (Shader::Gcn::GcnDecodeContext)
// and converts each decoded GcnInst into Kyty's own Decoder::Instruction, so
// Kyty's existing CFG / IR / SPIR-V pipeline can consume PS4 shaders without
// any changes to those downstream passes.
//
// Design: the GCN decoder is kept in its own isolated namespace
// (Shader::Gcn) with its own opcode tables (opcodes.h). This bridge is the
// only place that touches both worlds. Opcode translation is best-effort and
// intentionally conservative: well-known opcodes with a direct Kyty
// equivalent are mapped; everything else is marked Opcode::Unsupported with
// the upstream GCN opcode name recorded as the reason, which surfaces in the
// ReportPs4CompatGap diagnostic as a concrete work order.
#pragma once

#include "common/common.h"

#include "graphics/shader/recompiler/ShaderRecompiler.h"
#include "graphics/shader/recompiler/frontend/decode/ShaderDecoder.h"

#include <span>
#include <string>
#include <string_view>

namespace Libs::Graphics::ShaderRecompiler::Decoder {

// Decode a PS4 (GCN) shader binary into the Kyty Decoder::Program form.
// Returns false and fills `error` on a hard decode failure (empty input,
// out-of-bounds instruction). Individual unsupported opcodes do NOT fail
// the decode; they are recorded as Opcode::Unsupported on the relevant
// Instruction so the gap can be reported.
bool DecodeGcnProgram(std::span<const uint32_t> code, Program& program,
                      std::string* error);

} // namespace Libs::Graphics::ShaderRecompiler::Decoder
