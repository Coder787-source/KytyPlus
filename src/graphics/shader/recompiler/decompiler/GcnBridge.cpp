// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "graphics/shader/recompiler/decompiler/GcnBridge.h"

#include "common/logging/log.h"

#include <fmt/format.h>
#include <magic_enum.hpp>

// Ported shadPS4 GCN decoder. The gcn/ include root is private to this
// translation unit via the gcn_decoder target's include path; here we reach
// it through the project include root as well, so the relative includes
// inside the ported headers resolve to the local shims.
#include "shader_recompiler/frontend/decode.h"
#include "shader_recompiler/frontend/instruction.h"
#include "shader_recompiler/frontend/opcodes.h"

#include <cstring>

namespace Libs::Graphics::ShaderRecompiler::Decoder {
namespace {

using GcnInst = Shader::Gcn::GcnInst;
using GcnEncoding = Shader::Gcn::InstEncoding;
using GcnField = Shader::Gcn::OperandField;
using GcnOpcode = Shader::Gcn::Opcode;

// Map a GCN instruction encoding to Kyty's Family enum.
Family MapFamily(GcnEncoding enc) {
	switch (enc) {
		case GcnEncoding::SOP1: return Family::SOP1;
		case GcnEncoding::SOP2: return Family::SOP2;
		case GcnEncoding::SOPK: return Family::SOPK;
		case GcnEncoding::SOPC: return Family::SOPC;
		case GcnEncoding::SOPP: return Family::SOPP;
		case GcnEncoding::VOP1: return Family::VOP1;
		case GcnEncoding::VOP2: return Family::VOP2;
		case GcnEncoding::VOP3: return Family::VOP3;
		case GcnEncoding::VOP3P: return Family::VOP3P;
		case GcnEncoding::VOPC: return Family::VOPC;
		case GcnEncoding::VINTRP: return Family::VINTRP;
		case GcnEncoding::SMRD: return Family::SMEM;
		case GcnEncoding::MUBUF: return Family::MUBUF;
		case GcnEncoding::MTBUF: return Family::MTBUF;
		case GcnEncoding::MIMG: return Family::MIMG;
		case GcnEncoding::DS: return Family::DS;
		case GcnEncoding::EXP: return Family::EXP;
		default: return Family::Unknown;
	}
}

// Map a GCN operand (field + code) to Kyty's Operand form.
Operand MapOperand(const Shader::Gcn::InstOperand& src) {
	Operand op{};
	const auto field = src.field;
	const u32 code = src.code;

	switch (field) {
		case GcnField::ScalarGPR:
			op.kind = OperandKind::Sgpr;
			op.reg = code;
			break;
		case GcnField::VccLo:
			op.kind = OperandKind::VccLo;
			break;
		case GcnField::VccHi:
			op.kind = OperandKind::VccHi;
			break;
		case GcnField::VccZ:
			op.kind = OperandKind::VccZ;
			break;
		case GcnField::M0:
			op.kind = OperandKind::M0;
			break;
		case GcnField::ExecLo:
			op.kind = OperandKind::ExecLo;
			break;
		case GcnField::ExecHi:
			op.kind = OperandKind::ExecHi;
			break;
		case GcnField::ExecZ:
			op.kind = OperandKind::ExecZ;
			break;
		case GcnField::Scc:
			op.kind = OperandKind::Scc;
			break;
		case GcnField::ConstZero:
			op.kind = OperandKind::IntegerInlineConstant;
			op.signed_val = 0;
			op.value = 0;
			break;
		case GcnField::SignedConstIntPos:
			// 129..192 -> 0..63
			op.kind = OperandKind::IntegerInlineConstant;
			op.signed_val = static_cast<int32_t>(code - 129u);
			op.value = op.signed_val;
			break;
		case GcnField::SignedConstIntNeg:
			// 193..208 -> -1..-16
			op.kind = OperandKind::IntegerInlineConstant;
			op.signed_val = -static_cast<int32_t>(code - 192u);
			op.value = op.signed_val;
			break;
		case GcnField::ConstFloatPos_0_5:
			op.kind = OperandKind::FloatInlineConstant;
			op.float_val = 0.5f;
			break;
		case GcnField::ConstFloatNeg_0_5:
			op.kind = OperandKind::FloatInlineConstant;
			op.float_val = -0.5f;
			break;
		case GcnField::ConstFloatPos_1_0:
			op.kind = OperandKind::FloatInlineConstant;
			op.float_val = 1.0f;
			break;
		case GcnField::ConstFloatNeg_1_0:
			op.kind = OperandKind::FloatInlineConstant;
			op.float_val = -1.0f;
			break;
		case GcnField::ConstFloatPos_2_0:
			op.kind = OperandKind::FloatInlineConstant;
			op.float_val = 2.0f;
			break;
		case GcnField::ConstFloatNeg_2_0:
			op.kind = OperandKind::FloatInlineConstant;
			op.float_val = -2.0f;
			break;
		case GcnField::ConstFloatPos_4_0:
			op.kind = OperandKind::FloatInlineConstant;
			op.float_val = 4.0f;
			break;
		case GcnField::ConstFloatNeg_4_0:
			op.kind = OperandKind::FloatInlineConstant;
			op.float_val = -4.0f;
			break;
		case GcnField::Inv2Pi:
			op.kind = OperandKind::FloatInlineConstant;
			op.float_val = 0.15915494f; // 1/(2*pi)
			break;
		case GcnField::LiteralConst:
			op.kind = OperandKind::LiteralConstant;
			op.value = code;
			op.signed_val = static_cast<int32_t>(code);
			op.float_val = std::bit_cast<float>(code);
			break;
		case GcnField::VectorGPR:
			op.kind = OperandKind::Vgpr;
			op.reg = code - 256u;
			break;
		case GcnField::Sdwa:
		case GcnField::Dpp:
		case GcnField::LdsDirect:
		case GcnField::Undefined:
		default:
			op.kind = OperandKind::Unknown;
			break;
	}

	// Modifiers (input modifiers on sources).
	op.negate = src.input_modifier.neg;
	op.negate_hi = src.input_modifier.neg_hi;
	op.absolute = src.input_modifier.abs;
	op.op_sel = src.op_sel.op_sel;
	op.op_sel_hi = src.op_sel.op_sel_hi;
	if (src.dpp.has_value()) {
		op.dpp = true;
		op.dpp_ctrl = src.dpp->op;
		op.dpp_row_mask = src.dpp->row_mask;
		op.dpp_bank_mask = src.dpp->bank_mask;
		op.dpp_bound_ctrl = src.dpp->bc;
	}
	return op;
}

// Best-effort mapping of the GCN uniform Opcode to Kyty's Opcode. Direct
// correspondences are mapped; the long tail is intentionally left as
// Unsupported so the gap report surfaces them as concrete work items.
Opcode MapOpcode(GcnOpcode op) {
	switch (op) {
		case GcnOpcode::S_ADD_U32: return Opcode::SAddU32;
		case GcnOpcode::S_SUB_U32: return Opcode::SSubU32;
		case GcnOpcode::S_ADD_I32: return Opcode::SAddI32;
		case GcnOpcode::S_SUB_I32: return Opcode::SSubI32;
		case GcnOpcode::S_ADDC_U32: return Opcode::SAddcU32;
		case GcnOpcode::S_SUBB_U32: return Opcode::SSubbU32;
		case GcnOpcode::S_MIN_I32: return Opcode::SMinI32;
		case GcnOpcode::S_MIN_U32: return Opcode::SMinU32;
		case GcnOpcode::S_MAX_I32: return Opcode::SMaxI32;
		case GcnOpcode::S_MAX_U32: return Opcode::SMaxU32;
		case GcnOpcode::S_CSELECT_B32: return Opcode::SCselectB32;
		case GcnOpcode::S_CSELECT_B64: return Opcode::SCselectB64;
		case GcnOpcode::S_AND_B32: return Opcode::SAndB32;
		case GcnOpcode::S_AND_B64: return Opcode::SAndB64;
		case GcnOpcode::S_OR_B32: return Opcode::SOrB32;
		case GcnOpcode::S_OR_B64: return Opcode::SOrB64;
		case GcnOpcode::S_XOR_B32: return Opcode::SXorB32;
		case GcnOpcode::S_XOR_B64: return Opcode::SXorB64;
		case GcnOpcode::S_ANDN2_B32: return Opcode::SAndn2B32;
		case GcnOpcode::S_ANDN2_B64: return Opcode::SAndn2B64;
		case GcnOpcode::S_ORN2_B32: return Opcode::SOrn2B32;
		case GcnOpcode::S_ORN2_B64: return Opcode::SOrn2B64;
		case GcnOpcode::S_NAND_B32: return Opcode::SNandB32;
		case GcnOpcode::S_NAND_B64: return Opcode::SNandB64;
		case GcnOpcode::S_NOR_B32: return Opcode::SNorB32;
		case GcnOpcode::S_NOR_B64: return Opcode::SNorB64;
		case GcnOpcode::S_XNOR_B32: return Opcode::SXnorB32;
		case GcnOpcode::S_XNOR_B64: return Opcode::SXnorB64;
		case GcnOpcode::S_LSHL_B32: return Opcode::SLshlB32;
		case GcnOpcode::S_LSHL_B64: return Opcode::SLshlB64;
		case GcnOpcode::S_LSHR_B32: return Opcode::SLshrB32;
		case GcnOpcode::S_LSHR_B64: return Opcode::SLshrB64;
		case GcnOpcode::S_ASHR_I32: return Opcode::SAshrI32;
		case GcnOpcode::S_BFM_B32: return Opcode::SBfmB32;
		case GcnOpcode::S_BFM_B64: return Opcode::SBfmB64;
		case GcnOpcode::S_MUL_I32: return Opcode::SMulI32;
		case GcnOpcode::S_BFE_U32: return Opcode::SBfeU32;
		case GcnOpcode::S_BFE_U64: return Opcode::SBfeU64;
		case GcnOpcode::S_MOVK_I32: return Opcode::SMovkI32;
		case GcnOpcode::S_MULK_I32: return Opcode::SMulkI32;
		case GcnOpcode::S_SETREG_B32: return Opcode::SSetregB32;
		case GcnOpcode::S_MOV_B32: return Opcode::SMovB32;
		case GcnOpcode::S_MOV_B64: return Opcode::SMovB64;
		case GcnOpcode::S_NOT_B32: return Opcode::SNotB32;
		case GcnOpcode::S_NOT_B64: return Opcode::SNotB64;
		case GcnOpcode::S_WQM_B64: return Opcode::SWqmB64;
		case GcnOpcode::S_BREV_B32: return Opcode::SBrevB32;
		case GcnOpcode::S_BCNT1_I32_B32: return Opcode::SBcnt1I32B32;
		case GcnOpcode::S_BCNT1_I32_B64: return Opcode::SBcnt1I32B64;
		case GcnOpcode::S_FF1_I32_B32: return Opcode::SFf1I32B32;
		case GcnOpcode::S_FLBIT_I32_B64: return Opcode::SFlbitI32B64;
		case GcnOpcode::S_BITSET0_B32: return Opcode::SBitset0B32;
		case GcnOpcode::S_BITSET1_B32: return Opcode::SBitset1B32;
		case GcnOpcode::S_GETPC_B64: return Opcode::SGetpcB64;
		case GcnOpcode::S_SETPC_B64: return Opcode::SSetpcB64;
		case GcnOpcode::S_AND_SAVEEXEC_B64: return Opcode::SAndSaveexecB64;
		case GcnOpcode::S_ORN2_SAVEEXEC_B64: return Opcode::SOrn2SaveexecB64;
		case GcnOpcode::S_ABS_I32: return Opcode::SAbsI32;
		case GcnOpcode::S_CMP_EQ_I32: return Opcode::SCmpEqI32;
		case GcnOpcode::S_CMP_LG_I32: return Opcode::SCmpLgI32;
		case GcnOpcode::S_CMP_GT_I32: return Opcode::SCmpGtI32;
		case GcnOpcode::S_CMP_GE_I32: return Opcode::SCmpGeI32;
		case GcnOpcode::S_CMP_LT_I32: return Opcode::SCmpLtI32;
		case GcnOpcode::S_CMP_LE_I32: return Opcode::SCmpLeI32;
		case GcnOpcode::S_CMP_EQ_U32: return Opcode::SCmpEqU32;
		case GcnOpcode::S_CMP_LG_U32: return Opcode::SCmpLgU32;
		case GcnOpcode::S_CMP_GT_U32: return Opcode::SCmpGtU32;
		case GcnOpcode::S_CMP_GE_U32: return Opcode::SCmpGeU32;
		case GcnOpcode::S_CMP_LT_U32: return Opcode::SCmpLtU32;
		case GcnOpcode::S_CMP_LE_U32: return Opcode::SCmpLeU32;
		case GcnOpcode::S_BITCMP0_B32: return Opcode::SBitcmp0B32;
		case GcnOpcode::S_BITCMP1_B32: return Opcode::SBitcmp1B32;
		case GcnOpcode::S_NOP: return Opcode::SNop;
		case GcnOpcode::S_ENDPGM: return Opcode::SEndpgm;
		case GcnOpcode::S_BRANCH: return Opcode::SBranch;
		case GcnOpcode::S_CBRANCH_SCC0: return Opcode::SCbranchScc0;
		case GcnOpcode::S_CBRANCH_SCC1: return Opcode::SCbranchScc1;
		case GcnOpcode::S_CBRANCH_VCCZ: return Opcode::SCbranchVccz;
		case GcnOpcode::S_CBRANCH_VCCNZ: return Opcode::SCbranchVccnz;
		case GcnOpcode::S_CBRANCH_EXECZ: return Opcode::SCbranchExecz;
		case GcnOpcode::S_CBRANCH_EXECNZ: return Opcode::SCbranchExecnz;
		case GcnOpcode::S_BARRIER: return Opcode::SBarrier;
		case GcnOpcode::S_WAITCNT: return Opcode::SWaitcnt;
		case GcnOpcode::S_SLEEP: return Opcode::SSleep;
		case GcnOpcode::S_SENDMSG: return Opcode::SSendmsg;
		case GcnOpcode::V_CMP_F_F32: return Opcode::VCmpFF32;
		case GcnOpcode::V_CMP_LT_F32: return Opcode::VCmpLtF32;
		case GcnOpcode::V_CMP_EQ_F32: return Opcode::VCmpEqF32;
		case GcnOpcode::V_CMP_LE_F32: return Opcode::VCmpLeF32;
		case GcnOpcode::V_CMP_GT_F32: return Opcode::VCmpGtF32;
		case GcnOpcode::V_CMP_LG_F32: return Opcode::VCmpLgF32;
		case GcnOpcode::V_CMP_GE_F32: return Opcode::VCmpGeF32;
		case GcnOpcode::V_CMP_O_F32: return Opcode::VCmpOF32;
		case GcnOpcode::V_CMP_U_F32: return Opcode::VCmpUF32;
		case GcnOpcode::V_CMP_NGE_F32: return Opcode::VCmpNgeF32;
		case GcnOpcode::V_CMP_NLG_F32: return Opcode::VCmpNlgF32;
		case GcnOpcode::V_CMP_NGT_F32: return Opcode::VCmpNgtF32;
		case GcnOpcode::V_CMP_NLE_F32: return Opcode::VCmpNleF32;
		case GcnOpcode::V_CMP_NEQ_F32: return Opcode::VCmpNeqF32;
		case GcnOpcode::V_CMP_NLT_F32: return Opcode::VCmpNltF32;
		case GcnOpcode::V_CMP_TRU_F32: return Opcode::VCmpTruF32;
		case GcnOpcode::V_CMPX_LT_F32: return Opcode::VCmpxLtF32;
		case GcnOpcode::V_CMPX_EQ_F32: return Opcode::VCmpxEqF32;
		case GcnOpcode::V_CMPX_LE_F32: return Opcode::VCmpxLeF32;
		case GcnOpcode::V_CMPX_GT_F32: return Opcode::VCmpxGtF32;
		case GcnOpcode::V_CMPX_LG_F32: return Opcode::VCmpxLgF32;
		case GcnOpcode::V_CMPX_GE_F32: return Opcode::VCmpxGeF32;
		case GcnOpcode::V_CMPX_NGE_F32: return Opcode::VCmpxNgeF32;
		case GcnOpcode::V_CMPX_NLG_F32: return Opcode::VCmpxNlgF32;
		case GcnOpcode::V_CMPX_NGT_F32: return Opcode::VCmpxNgtF32;
		case GcnOpcode::V_CMPX_NLE_F32: return Opcode::VCmpxNleF32;
		case GcnOpcode::V_CMPX_NEQ_F32: return Opcode::VCmpxNeqF32;
		case GcnOpcode::V_CMPX_NLT_F32: return Opcode::VCmpxNltF32;
		case GcnOpcode::V_CMP_F_I32: return Opcode::VCmpFI32;
		case GcnOpcode::V_CMP_LT_I32: return Opcode::VCmpLtI32;
		case GcnOpcode::V_CMP_EQ_I32: return Opcode::VCmpEqI32;
		case GcnOpcode::V_CMP_LE_I32: return Opcode::VCmpLeI32;
		case GcnOpcode::V_CMP_GT_I32: return Opcode::VCmpGtI32;
		case GcnOpcode::V_CMP_NE_I32: return Opcode::VCmpNeI32;
		case GcnOpcode::V_CMP_GE_I32: return Opcode::VCmpGeI32;
		case GcnOpcode::V_CMP_T_I32: return Opcode::VCmpTI32;
		case GcnOpcode::V_CMP_CLASS_F32: return Opcode::VCmpClassF32;
		case GcnOpcode::V_CMP_LT_I16: return Opcode::VCmpLtI16;
		case GcnOpcode::V_CMP_EQ_I16: return Opcode::VCmpEqI16;
		case GcnOpcode::V_CMP_LE_I16: return Opcode::VCmpLeI16;
		case GcnOpcode::V_CMP_GT_I16: return Opcode::VCmpGtI16;
		case GcnOpcode::V_CMP_NE_I16: return Opcode::VCmpNeI16;
		case GcnOpcode::V_CMP_GE_I16: return Opcode::VCmpGeI16;
		case GcnOpcode::V_CMPX_LT_I32: return Opcode::VCmpxLtI32;
		case GcnOpcode::V_CMPX_EQ_I32: return Opcode::VCmpxEqI32;
		case GcnOpcode::V_CMPX_LE_I32: return Opcode::VCmpxLeI32;
		case GcnOpcode::V_CMPX_GT_I32: return Opcode::VCmpxGtI32;
		case GcnOpcode::V_CMPX_NE_I32: return Opcode::VCmpxNeI32;
		case GcnOpcode::V_CMPX_GE_I32: return Opcode::VCmpxGeI32;
		case GcnOpcode::V_CMP_LT_U16: return Opcode::VCmpLtU16;
		case GcnOpcode::V_CMP_EQ_U16: return Opcode::VCmpEqU16;
		case GcnOpcode::V_CMP_LE_U16: return Opcode::VCmpLeU16;
		case GcnOpcode::V_CMP_GT_U16: return Opcode::VCmpGtU16;
		case GcnOpcode::V_CMP_NE_U16: return Opcode::VCmpNeU16;
		case GcnOpcode::V_CMP_GE_U16: return Opcode::VCmpGeU16;
		case GcnOpcode::V_CMP_F_U32: return Opcode::VCmpFU32;
		case GcnOpcode::V_CMP_LT_U32: return Opcode::VCmpLtU32;
		case GcnOpcode::V_CMP_EQ_U32: return Opcode::VCmpEqU32;
		case GcnOpcode::V_CMP_LE_U32: return Opcode::VCmpLeU32;
		case GcnOpcode::V_CMP_GT_U32: return Opcode::VCmpGtU32;
		case GcnOpcode::V_CMP_NE_U32: return Opcode::VCmpNeU32;
		case GcnOpcode::V_CMP_GE_U32: return Opcode::VCmpGeU32;
		case GcnOpcode::V_CMP_T_U32: return Opcode::VCmpTU32;
		case GcnOpcode::V_CMP_LT_F16: return Opcode::VCmpLtF16;
		case GcnOpcode::V_CMP_EQ_F16: return Opcode::VCmpEqF16;
		case GcnOpcode::V_CMP_LE_F16: return Opcode::VCmpLeF16;
		case GcnOpcode::V_CMP_GT_F16: return Opcode::VCmpGtF16;
		case GcnOpcode::V_CMP_LG_F16: return Opcode::VCmpLgF16;
		case GcnOpcode::V_CMP_GE_F16: return Opcode::VCmpGeF16;
		case GcnOpcode::V_CMPX_LT_U32: return Opcode::VCmpxLtU32;
		case GcnOpcode::V_CMPX_EQ_U32: return Opcode::VCmpxEqU32;
		case GcnOpcode::V_CMPX_LE_U32: return Opcode::VCmpxLeU32;
		case GcnOpcode::V_CMPX_GT_U32: return Opcode::VCmpxGtU32;
		case GcnOpcode::V_CMPX_NE_U32: return Opcode::VCmpxNeU32;
		case GcnOpcode::V_CMPX_GE_U32: return Opcode::VCmpxGeU32;
		case GcnOpcode::V_CMP_NE_U64: return Opcode::VCmpNeU64;
		case GcnOpcode::V_CNDMASK_B32: return Opcode::VCndmaskB32;
		case GcnOpcode::V_READLANE_B32: return Opcode::VReadlaneB32;
		case GcnOpcode::V_WRITELANE_B32: return Opcode::VWritelaneB32;
		case GcnOpcode::V_ADD_F32: return Opcode::VAddF32;
		case GcnOpcode::V_SUB_F32: return Opcode::VSubF32;
		case GcnOpcode::V_SUBREV_F32: return Opcode::VSubrevF32;
		case GcnOpcode::V_MUL_F32: return Opcode::VMulF32;
		case GcnOpcode::V_MUL_I32_I24: return Opcode::VMulI32I24;
		case GcnOpcode::V_MUL_U32_U24: return Opcode::VMulU32U24;
		case GcnOpcode::V_MIN_F32: return Opcode::VMinF32;
		case GcnOpcode::V_MAX_F32: return Opcode::VMaxF32;
		case GcnOpcode::V_MIN_I32: return Opcode::VMinI32;
		case GcnOpcode::V_MAX_I32: return Opcode::VMaxI32;
		case GcnOpcode::V_MIN_U32: return Opcode::VMinU32;
		case GcnOpcode::V_MAX_U32: return Opcode::VMaxU32;
		case GcnOpcode::V_LSHR_B32: return Opcode::VLshrB32;
		case GcnOpcode::V_LSHRREV_B32: return Opcode::VLshrrevB32;
		case GcnOpcode::V_ASHR_I32: return Opcode::VAshrI32;
		case GcnOpcode::V_ASHRREV_I32: return Opcode::VAshrrevI32;
		case GcnOpcode::V_LSHL_B32: return Opcode::VLshlB32;
		case GcnOpcode::V_LSHLREV_B32: return Opcode::VLshlrevB32;
		case GcnOpcode::V_AND_B32: return Opcode::VAndB32;
		case GcnOpcode::V_OR_B32: return Opcode::VOrB32;
		case GcnOpcode::V_XOR_B32: return Opcode::VXorB32;
		case GcnOpcode::V_BFM_B32: return Opcode::VBfmB32;
		case GcnOpcode::V_MAC_F32: return Opcode::VMacF32;
		case GcnOpcode::V_MADMK_F32: return Opcode::VMadmkF32;
		case GcnOpcode::V_MADAK_F32: return Opcode::VMadakF32;
		case GcnOpcode::V_BCNT_U32_B32: return Opcode::VBcntU32B32;
		case GcnOpcode::V_MBCNT_LO_U32_B32: return Opcode::VMbcntLoU32B32;
		case GcnOpcode::V_MBCNT_HI_U32_B32: return Opcode::VMbcntHiU32B32;
		case GcnOpcode::V_ADD_I32: return Opcode::VAddI32;
		case GcnOpcode::V_SUB_I32: return Opcode::VSubI32;
		case GcnOpcode::V_SUBREV_I32: return Opcode::VSubrevI32;
		case GcnOpcode::V_ADDC_U32: return Opcode::VAddcU32;
		case GcnOpcode::V_LDEXP_F32: return Opcode::VLdexpF32;
		case GcnOpcode::V_CVT_PKNORM_I16_F32: return Opcode::VCvtPknormI16F32;
		case GcnOpcode::V_CVT_PKNORM_U16_F32: return Opcode::VCvtPknormU16F32;
		case GcnOpcode::V_CVT_PKRTZ_F16_F32: return Opcode::VCvtPkrtzF16F32;
		case GcnOpcode::V_CVT_PK_U16_U32: return Opcode::VCvtPkU16U32;
		case GcnOpcode::V_ADD_F16: return Opcode::VAddF16;
		case GcnOpcode::V_SUB_F16: return Opcode::VSubF16;
		case GcnOpcode::V_SUBREV_F16: return Opcode::VSubrevF16;
		case GcnOpcode::V_MUL_F16: return Opcode::VMulF16;
		case GcnOpcode::V_MAX_F16: return Opcode::VMaxF16;
		case GcnOpcode::V_MIN_F16: return Opcode::VMinF16;
		case GcnOpcode::V_NOP: return Opcode::VNop;
		case GcnOpcode::V_MOV_B32: return Opcode::VMovB32;
		case GcnOpcode::V_READFIRSTLANE_B32: return Opcode::VReadfirstlaneB32;
		case GcnOpcode::V_CVT_F32_I32: return Opcode::VCvtF32I32;
		case GcnOpcode::V_CVT_F32_U32: return Opcode::VCvtF32U32;
		case GcnOpcode::V_CVT_U32_F32: return Opcode::VCvtU32F32;
		case GcnOpcode::V_CVT_I32_F32: return Opcode::VCvtI32F32;
		case GcnOpcode::V_CVT_F16_F32: return Opcode::VCvtF16F32;
		case GcnOpcode::V_CVT_F32_F16: return Opcode::VCvtF32F16;
		case GcnOpcode::V_CVT_RPI_I32_F32: return Opcode::VCvtRpiI32F32;
		case GcnOpcode::V_CVT_FLR_I32_F32: return Opcode::VCvtFlrI32F32;
		case GcnOpcode::V_CVT_OFF_F32_I4: return Opcode::VCvtOffF32I4;
		case GcnOpcode::V_CVT_F32_UBYTE0: return Opcode::VCvtF32Ubyte0;
		case GcnOpcode::V_CVT_F32_UBYTE1: return Opcode::VCvtF32Ubyte1;
		case GcnOpcode::V_CVT_F32_UBYTE2: return Opcode::VCvtF32Ubyte2;
		case GcnOpcode::V_CVT_F32_UBYTE3: return Opcode::VCvtF32Ubyte3;
		case GcnOpcode::V_FRACT_F32: return Opcode::VFractF32;
		case GcnOpcode::V_TRUNC_F32: return Opcode::VTruncF32;
		case GcnOpcode::V_CEIL_F32: return Opcode::VCeilF32;
		case GcnOpcode::V_RNDNE_F32: return Opcode::VRndneF32;
		case GcnOpcode::V_FLOOR_F32: return Opcode::VFloorF32;
		case GcnOpcode::V_EXP_F32: return Opcode::VExpF32;
		case GcnOpcode::V_LOG_F32: return Opcode::VLogF32;
		case GcnOpcode::V_RCP_F32: return Opcode::VRcpF32;
		case GcnOpcode::V_RSQ_F32: return Opcode::VRsqF32;
		case GcnOpcode::V_SQRT_F32: return Opcode::VSqrtF32;
		case GcnOpcode::V_SIN_F32: return Opcode::VSinF32;
		case GcnOpcode::V_COS_F32: return Opcode::VCosF32;
		case GcnOpcode::V_NOT_B32: return Opcode::VNotB32;
		case GcnOpcode::V_BFREV_B32: return Opcode::VBfrevB32;
		case GcnOpcode::V_FFBH_U32: return Opcode::VFfbhU32;
		case GcnOpcode::V_FFBL_B32: return Opcode::VFfblB32;
		case GcnOpcode::V_MOVRELD_B32: return Opcode::VMovreldB32;
		case GcnOpcode::V_MOVRELS_B32: return Opcode::VMovrelsB32;
		case GcnOpcode::V_RCP_F16: return Opcode::VRcpF16;
		case GcnOpcode::V_SQRT_F16: return Opcode::VSqrtF16;
		case GcnOpcode::V_MAD_F32: return Opcode::VMadF32;
		case GcnOpcode::V_MAD_I32_I24: return Opcode::VMadI32I24;
		case GcnOpcode::V_MAD_U32_U24: return Opcode::VMadU32U24;
		case GcnOpcode::V_CUBEID_F32: return Opcode::VCubeidF32;
		case GcnOpcode::V_CUBESC_F32: return Opcode::VCubescF32;
		case GcnOpcode::V_CUBETC_F32: return Opcode::VCubetcF32;
		case GcnOpcode::V_CUBEMA_F32: return Opcode::VCubemaF32;
		case GcnOpcode::V_BFE_U32: return Opcode::VBfeU32;
		case GcnOpcode::V_BFE_I32: return Opcode::VBfeI32;
		case GcnOpcode::V_BFI_B32: return Opcode::VBfiB32;
		case GcnOpcode::V_FMA_F32: return Opcode::VFmaF32;
		case GcnOpcode::V_ALIGNBIT_B32: return Opcode::VAlignbitB32;
		case GcnOpcode::V_MIN3_F32: return Opcode::VMin3F32;
		case GcnOpcode::V_MIN3_I32: return Opcode::VMin3I32;
		case GcnOpcode::V_MIN3_U32: return Opcode::VMin3U32;
		case GcnOpcode::V_MAX3_F32: return Opcode::VMax3F32;
		case GcnOpcode::V_MAX3_I32: return Opcode::VMax3I32;
		case GcnOpcode::V_MAX3_U32: return Opcode::VMax3U32;
		case GcnOpcode::V_MED3_F32: return Opcode::VMed3F32;
		case GcnOpcode::V_MED3_I32: return Opcode::VMed3I32;
		case GcnOpcode::V_MED3_U32: return Opcode::VMed3U32;
		case GcnOpcode::V_SAD_U32: return Opcode::VSadU32;
		case GcnOpcode::V_CVT_PK_U8_F32: return Opcode::VCvtPkU8F32;
		case GcnOpcode::V_MUL_LO_U32: return Opcode::VMulLoU32;
		case GcnOpcode::V_MUL_HI_U32: return Opcode::VMulHiU32;
		case GcnOpcode::V_MUL_LO_I32: return Opcode::VMulLoI32;
		case GcnOpcode::V_MUL_HI_I32: return Opcode::VMulHiI32;
		case GcnOpcode::V_MAD_U64_U32: return Opcode::VMadU64U32;
		case GcnOpcode::V_ADD_NC_U16: return Opcode::VAddNcU16;
		case GcnOpcode::V_LSHRREV_B16: return Opcode::VLshrrevB16;
		case GcnOpcode::V_ASHRREV_I16: return Opcode::VAshrrevI16;
		case GcnOpcode::V_ADD_NC_I16: return Opcode::VAddNcI16;
		case GcnOpcode::V_SUB_NC_I16: return Opcode::VSubNcI16;
		case GcnOpcode::V_LSHLREV_B16: return Opcode::VLshlrevB16;
		case GcnOpcode::V_LSHL_ADD_U32: return Opcode::VLshlAddU32;
		case GcnOpcode::V_ADD_LSHL_U32: return Opcode::VAddLshlU32;
		case GcnOpcode::V_MIN3_F16: return Opcode::VMin3F16;
		case GcnOpcode::V_MAX3_F16: return Opcode::VMax3F16;
		case GcnOpcode::V_MED3_F16: return Opcode::VMed3F16;
		case GcnOpcode::V_ADD3_U32: return Opcode::VAdd3U32;
		case GcnOpcode::V_LSHL_OR_B32: return Opcode::VLshlOrB32;
		case GcnOpcode::V_AND_OR_B32: return Opcode::VAndOrB32;
		case GcnOpcode::V_OR3_B32: return Opcode::VOr3B32;
		case GcnOpcode::V_INTERP_P1_F32: return Opcode::VInterpP1F32;
		case GcnOpcode::V_INTERP_P2_F32: return Opcode::VInterpP2F32;
		case GcnOpcode::V_INTERP_MOV_F32: return Opcode::VInterpMovF32;
		case GcnOpcode::S_LOAD_DWORD: return Opcode::SLoadDword;
		case GcnOpcode::S_LOAD_DWORDX2: return Opcode::SLoadDwordx2;
		case GcnOpcode::S_LOAD_DWORDX4: return Opcode::SLoadDwordx4;
		case GcnOpcode::S_LOAD_DWORDX8: return Opcode::SLoadDwordx8;
		case GcnOpcode::S_LOAD_DWORDX16: return Opcode::SLoadDwordx16;
		case GcnOpcode::S_BUFFER_LOAD_DWORD: return Opcode::SBufferLoadDword;
		case GcnOpcode::S_BUFFER_LOAD_DWORDX2: return Opcode::SBufferLoadDwordx2;
		case GcnOpcode::S_BUFFER_LOAD_DWORDX4: return Opcode::SBufferLoadDwordx4;
		case GcnOpcode::S_BUFFER_LOAD_DWORDX8: return Opcode::SBufferLoadDwordx8;
		case GcnOpcode::S_BUFFER_LOAD_DWORDX16: return Opcode::SBufferLoadDwordx16;
		case GcnOpcode::DS_ADD_U32: return Opcode::DsAddU32;
		case GcnOpcode::DS_SUB_U32: return Opcode::DsSubU32;
		case GcnOpcode::DS_MIN_I32: return Opcode::DsMinI32;
		case GcnOpcode::DS_MAX_I32: return Opcode::DsMaxI32;
		case GcnOpcode::DS_MIN_U32: return Opcode::DsMinU32;
		case GcnOpcode::DS_MAX_U32: return Opcode::DsMaxU32;
		case GcnOpcode::DS_AND_B32: return Opcode::DsAndB32;
		case GcnOpcode::DS_OR_B32: return Opcode::DsOrB32;
		case GcnOpcode::DS_XOR_B32: return Opcode::DsXorB32;
		case GcnOpcode::DS_WRITE_B32: return Opcode::DsWriteB32;
		case GcnOpcode::DS_WRITE2_B32: return Opcode::DsWrite2B32;
		case GcnOpcode::DS_MIN_F32: return Opcode::DsMinF32;
		case GcnOpcode::DS_MAX_F32: return Opcode::DsMaxF32;
		case GcnOpcode::DS_ADD_RTN_U32: return Opcode::DsAddRtnU32;
		case GcnOpcode::DS_SUB_RTN_U32: return Opcode::DsSubRtnU32;
		case GcnOpcode::DS_MIN_RTN_I32: return Opcode::DsMinRtnI32;
		case GcnOpcode::DS_MAX_RTN_I32: return Opcode::DsMaxRtnI32;
		case GcnOpcode::DS_MIN_RTN_U32: return Opcode::DsMinRtnU32;
		case GcnOpcode::DS_MAX_RTN_U32: return Opcode::DsMaxRtnU32;
		case GcnOpcode::DS_AND_RTN_B32: return Opcode::DsAndRtnB32;
		case GcnOpcode::DS_OR_RTN_B32: return Opcode::DsOrRtnB32;
		case GcnOpcode::DS_XOR_RTN_B32: return Opcode::DsXorRtnB32;
		case GcnOpcode::DS_WRXCHG_RTN_B32: return Opcode::DsWrxchgRtnB32;
		case GcnOpcode::DS_SWIZZLE_B32: return Opcode::DsSwizzleB32;
		case GcnOpcode::DS_READ_B32: return Opcode::DsReadB32;
		case GcnOpcode::DS_READ2_B32: return Opcode::DsRead2B32;
		case GcnOpcode::DS_CONSUME: return Opcode::DsConsume;
		case GcnOpcode::DS_APPEND: return Opcode::DsAppend;
		case GcnOpcode::DS_WRITE_B64: return Opcode::DsWriteB64;
		case GcnOpcode::DS_WRITE2_B64: return Opcode::DsWrite2B64;
		case GcnOpcode::DS_READ_B64: return Opcode::DsReadB64;
		case GcnOpcode::DS_READ2_B64: return Opcode::DsRead2B64;
		case GcnOpcode::DS_WRITE_B96: return Opcode::DsWriteB96;
		case GcnOpcode::DS_WRITE_B128: return Opcode::DsWriteB128;
		case GcnOpcode::DS_READ_B96: return Opcode::DsReadB96;
		case GcnOpcode::DS_READ_B128: return Opcode::DsReadB128;
		case GcnOpcode::BUFFER_LOAD_FORMAT_X: return Opcode::BufferLoadFormatX;
		case GcnOpcode::BUFFER_LOAD_FORMAT_XY: return Opcode::BufferLoadFormatXy;
		case GcnOpcode::BUFFER_LOAD_FORMAT_XYZ: return Opcode::BufferLoadFormatXyz;
		case GcnOpcode::BUFFER_LOAD_FORMAT_XYZW: return Opcode::BufferLoadFormatXyzw;
		case GcnOpcode::BUFFER_STORE_FORMAT_X: return Opcode::BufferStoreFormatX;
		case GcnOpcode::BUFFER_STORE_FORMAT_XY: return Opcode::BufferStoreFormatXy;
		case GcnOpcode::BUFFER_STORE_FORMAT_XYZ: return Opcode::BufferStoreFormatXyz;
		case GcnOpcode::BUFFER_STORE_FORMAT_XYZW: return Opcode::BufferStoreFormatXyzw;
		case GcnOpcode::BUFFER_LOAD_UBYTE: return Opcode::BufferLoadUbyte;
		case GcnOpcode::BUFFER_LOAD_SBYTE: return Opcode::BufferLoadSbyte;
		case GcnOpcode::BUFFER_LOAD_USHORT: return Opcode::BufferLoadUshort;
		case GcnOpcode::BUFFER_LOAD_SSHORT: return Opcode::BufferLoadSshort;
		case GcnOpcode::BUFFER_LOAD_DWORD: return Opcode::BufferLoadDword;
		case GcnOpcode::BUFFER_LOAD_DWORDX2: return Opcode::BufferLoadDwordx2;
		case GcnOpcode::BUFFER_LOAD_DWORDX4: return Opcode::BufferLoadDwordx4;
		case GcnOpcode::BUFFER_LOAD_DWORDX3: return Opcode::BufferLoadDwordx3;
		case GcnOpcode::BUFFER_STORE_BYTE: return Opcode::BufferStoreByte;
		case GcnOpcode::BUFFER_STORE_SHORT: return Opcode::BufferStoreShort;
		case GcnOpcode::BUFFER_STORE_DWORD: return Opcode::BufferStoreDword;
		case GcnOpcode::BUFFER_STORE_DWORDX2: return Opcode::BufferStoreDwordx2;
		case GcnOpcode::BUFFER_STORE_DWORDX4: return Opcode::BufferStoreDwordx4;
		case GcnOpcode::BUFFER_STORE_DWORDX3: return Opcode::BufferStoreDwordx3;
		case GcnOpcode::BUFFER_ATOMIC_SWAP: return Opcode::BufferAtomicSwap;
		case GcnOpcode::BUFFER_ATOMIC_ADD: return Opcode::BufferAtomicAdd;
		case GcnOpcode::BUFFER_ATOMIC_SUB: return Opcode::BufferAtomicSub;
		case GcnOpcode::BUFFER_ATOMIC_AND: return Opcode::BufferAtomicAnd;
		case GcnOpcode::BUFFER_ATOMIC_OR: return Opcode::BufferAtomicOr;
		case GcnOpcode::BUFFER_ATOMIC_XOR: return Opcode::BufferAtomicXor;
		case GcnOpcode::IMAGE_LOAD: return Opcode::ImageLoad;
		case GcnOpcode::IMAGE_LOAD_MIP: return Opcode::ImageLoadMip;
		case GcnOpcode::IMAGE_STORE: return Opcode::ImageStore;
		case GcnOpcode::IMAGE_STORE_MIP: return Opcode::ImageStoreMip;
		case GcnOpcode::IMAGE_GET_RESINFO: return Opcode::ImageGetResinfo;
		case GcnOpcode::IMAGE_ATOMIC_ADD: return Opcode::ImageAtomicAdd;
		case GcnOpcode::IMAGE_ATOMIC_AND: return Opcode::ImageAtomicAnd;
		case GcnOpcode::IMAGE_ATOMIC_OR: return Opcode::ImageAtomicOr;
		case GcnOpcode::IMAGE_ATOMIC_XOR: return Opcode::ImageAtomicXor;
		case GcnOpcode::IMAGE_SAMPLE: return Opcode::ImageSample;
		case GcnOpcode::IMAGE_GATHER4_LZ: return Opcode::ImageGather4Lz;
		case GcnOpcode::IMAGE_GATHER4_C: return Opcode::ImageGather4C;
		case GcnOpcode::IMAGE_GATHER4_C_LZ: return Opcode::ImageGather4CLz;
		case GcnOpcode::IMAGE_GATHER4_LZ_O: return Opcode::ImageGather4LzO;
		case GcnOpcode::IMAGE_GATHER4_C_O: return Opcode::ImageGather4CO;
		case GcnOpcode::IMAGE_GATHER4_C_LZ_O: return Opcode::ImageGather4CLzO;
		case GcnOpcode::IMAGE_GET_LOD: return Opcode::ImageGetLod;
		case GcnOpcode::V_PK_MAD_I16: return Opcode::VPkMadI16;
		case GcnOpcode::V_PK_MUL_LO_U16: return Opcode::VPkMulLoU16;
		case GcnOpcode::V_PK_ADD_I16: return Opcode::VPkAddI16;
		case GcnOpcode::V_PK_SUB_I16: return Opcode::VPkSubI16;
		case GcnOpcode::V_PK_LSHLREV_B16: return Opcode::VPkLshlrevB16;
		case GcnOpcode::V_PK_LSHRREV_B16: return Opcode::VPkLshrrevB16;
		case GcnOpcode::V_PK_ASHRREV_I16: return Opcode::VPkAshrrevI16;
		case GcnOpcode::V_PK_MAX_I16: return Opcode::VPkMaxI16;
		case GcnOpcode::V_PK_MIN_I16: return Opcode::VPkMinI16;
		case GcnOpcode::V_PK_MAD_U16: return Opcode::VPkMadU16;
		case GcnOpcode::V_PK_ADD_U16: return Opcode::VPkAddU16;
		case GcnOpcode::V_PK_SUB_U16: return Opcode::VPkSubU16;
		case GcnOpcode::V_PK_MAX_U16: return Opcode::VPkMaxU16;
		case GcnOpcode::V_PK_MIN_U16: return Opcode::VPkMinU16;
		case GcnOpcode::V_PK_FMA_F16: return Opcode::VPkFmaF16;
		case GcnOpcode::V_PK_ADD_F16: return Opcode::VPkAddF16;
		case GcnOpcode::V_PK_MUL_F16: return Opcode::VPkMulF16;
		case GcnOpcode::V_PK_MIN_F16: return Opcode::VPkMinF16;
		case GcnOpcode::V_PK_MAX_F16: return Opcode::VPkMaxF16;
		case GcnOpcode::V_MAD_MIXLO_F16: return Opcode::VMadMixloF16;
		case GcnOpcode::V_MAD_MIXHI_F16: return Opcode::VMadMixhiF16;
		// The following GCN opcodes have no direct equivalent in Kyty's
		// RDNA2-oriented Opcode enum. They are intentionally left to fall
		// through to the default (Unsupported) so the gap report surfaces
		// them by name as concrete work items.
		case GcnOpcode::S_ASHR_I64: break;
		case GcnOpcode::S_BFE_I32: break;
		case GcnOpcode::S_BFE_I64: break;
		case GcnOpcode::S_CBRANCH_G_FORK: break;
		case GcnOpcode::S_ABSDIFF_I32: break;
		case GcnOpcode::S_CMOVK_I32: break;
		case GcnOpcode::S_CMPK_EQ_I32: break;
		case GcnOpcode::S_CMPK_LG_I32: break;
		case GcnOpcode::S_CMPK_GT_I32: break;
		case GcnOpcode::S_CMPK_GE_I32: break;
		case GcnOpcode::S_CMPK_LT_I32: break;
		case GcnOpcode::S_CMPK_LE_I32: break;
		case GcnOpcode::S_CMPK_EQ_U32: break;
		case GcnOpcode::S_CMPK_LG_U32: break;
		case GcnOpcode::S_CMPK_GT_U32: break;
		case GcnOpcode::S_CMPK_GE_U32: break;
		case GcnOpcode::S_CMPK_LT_U32: break;
		case GcnOpcode::S_CMPK_LE_U32: break;
		case GcnOpcode::S_ADDK_I32: break;
		case GcnOpcode::S_CBRANCH_I_FORK: break;
		case GcnOpcode::S_GETREG_B32: break;
		case GcnOpcode::S_GETREG_REGRD_B32: break;
		case GcnOpcode::S_SETREG_IMM32_B32: break;
		case GcnOpcode::S_CMOV_B32: break;
		case GcnOpcode::S_CMOV_B64: break;
		case GcnOpcode::S_WQM_B32: break;
		case GcnOpcode::S_BREV_B64: break;
		case GcnOpcode::S_BCNT0_I32_B32: break;
		case GcnOpcode::S_BCNT0_I32_B64: break;
		case GcnOpcode::S_FF0_I32_B32: break;
		case GcnOpcode::S_FF0_I32_B64: break;
		case GcnOpcode::S_FF1_I32_B64: break;
		case GcnOpcode::S_FLBIT_I32_B32: break;
		case GcnOpcode::S_FLBIT_I32: break;
		case GcnOpcode::S_FLBIT_I32_I64: break;
		case GcnOpcode::S_SEXT_I32_I8: break;
		case GcnOpcode::S_SEXT_I32_I16: break;
		case GcnOpcode::S_BITSET0_B64: break;
		case GcnOpcode::S_BITSET1_B64: break;
		case GcnOpcode::S_SWAPPC_B64: break;
		case GcnOpcode::S_RFE_B64: break;
		case GcnOpcode::S_OR_SAVEEXEC_B64: break;
		case GcnOpcode::S_XOR_SAVEEXEC_B64: break;
		case GcnOpcode::S_ANDN2_SAVEEXEC_B64: break;
		case GcnOpcode::S_NAND_SAVEEXEC_B64: break;
		case GcnOpcode::S_NOR_SAVEEXEC_B64: break;
		case GcnOpcode::S_XNOR_SAVEEXEC_B64: break;
		case GcnOpcode::S_QUADMASK_B32: break;
		case GcnOpcode::S_QUADMASK_B64: break;
		case GcnOpcode::S_MOVRELS_B32: break;
		case GcnOpcode::S_MOVRELS_B64: break;
		case GcnOpcode::S_MOVRELD_B32: break;
		case GcnOpcode::S_MOVRELD_B64: break;
		case GcnOpcode::S_CBRANCH_JOIN: break;
		case GcnOpcode::S_MOV_REGRD_B32: break;
		case GcnOpcode::S_MOV_FED_B32: break;
		case GcnOpcode::S_BITCMP0_B64: break;
		case GcnOpcode::S_BITCMP1_B64: break;
		case GcnOpcode::S_SETVSKIP: break;
		case GcnOpcode::S_SETKILL: break;
		case GcnOpcode::S_SETHALT: break;
		case GcnOpcode::S_SETPRIO: break;
		case GcnOpcode::S_SENDMSGHALT: break;
		case GcnOpcode::S_TRAP: break;
		case GcnOpcode::S_ICACHE_INV: break;
		case GcnOpcode::S_INCPERFLEVEL: break;
		case GcnOpcode::S_DECPERFLEVEL: break;
		case GcnOpcode::S_TTRACEDATA: break;
		case GcnOpcode::S_CBRANCH_CDBGSYS: break;
		case GcnOpcode::S_CBRANCH_CDBGUSER: break;
		case GcnOpcode::S_CBRANCH_CDBGSYS_OR_USER: break;
		case GcnOpcode::S_CBRANCH_CDBGSYS_AND_USER: break;
		case GcnOpcode::V_CMP_T_F32: break;
		case GcnOpcode::V_CMPX_F_F32: break;
		case GcnOpcode::V_CMPX_O_F32: break;
		case GcnOpcode::V_CMPX_U_F32: break;
		case GcnOpcode::V_CMPX_TRU_F32: break;
		case GcnOpcode::V_CMPX_T_F32: break;
		case GcnOpcode::V_CMP_F_F64: break;
		case GcnOpcode::V_CMP_LT_F64: break;
		case GcnOpcode::V_CMP_EQ_F64: break;
		case GcnOpcode::V_CMP_LE_F64: break;
		case GcnOpcode::V_CMP_GT_F64: break;
		case GcnOpcode::V_CMP_LG_F64: break;
		case GcnOpcode::V_CMP_GE_F64: break;
		case GcnOpcode::V_CMP_O_F64: break;
		case GcnOpcode::V_CMP_U_F64: break;
		case GcnOpcode::V_CMP_NGE_F64: break;
		case GcnOpcode::V_CMP_NLG_F64: break;
		case GcnOpcode::V_CMP_NGT_F64: break;
		case GcnOpcode::V_CMP_NLE_F64: break;
		case GcnOpcode::V_CMP_NEQ_F64: break;
		case GcnOpcode::V_CMP_NLT_F64: break;
		case GcnOpcode::V_CMP_TRU_F64: break;
		case GcnOpcode::V_CMP_T_F64: break;
		case GcnOpcode::V_CMPX_F_F64: break;
		case GcnOpcode::V_CMPX_LT_F64: break;
		case GcnOpcode::V_CMPX_EQ_F64: break;
		case GcnOpcode::V_CMPX_LE_F64: break;
		case GcnOpcode::V_CMPX_GT_F64: break;
		case GcnOpcode::V_CMPX_LG_F64: break;
		case GcnOpcode::V_CMPX_GE_F64: break;
		case GcnOpcode::V_CMPX_O_F64: break;
		case GcnOpcode::V_CMPX_U_F64: break;
		case GcnOpcode::V_CMPX_NGE_F64: break;
		case GcnOpcode::V_CMPX_NLG_F64: break;
		case GcnOpcode::V_CMPX_NGT_F64: break;
		case GcnOpcode::V_CMPX_NLE_F64: break;
		case GcnOpcode::V_CMPX_NEQ_F64: break;
		case GcnOpcode::V_CMPX_NLT_F64: break;
		case GcnOpcode::V_CMPX_TRU_F64: break;
		case GcnOpcode::V_CMPX_T_F64: break;
		case GcnOpcode::V_CMPS_F_F32: break;
		case GcnOpcode::V_CMPS_LT_F32: break;
		case GcnOpcode::V_CMPS_EQ_F32: break;
		case GcnOpcode::V_CMPS_LE_F32: break;
		case GcnOpcode::V_CMPS_GT_F32: break;
		case GcnOpcode::V_CMPS_LG_F32: break;
		case GcnOpcode::V_CMPS_GE_F32: break;
		case GcnOpcode::V_CMPS_O_F32: break;
		case GcnOpcode::V_CMPS_U_F32: break;
		case GcnOpcode::V_CMPS_NGE_F32: break;
		case GcnOpcode::V_CMPS_NLG_F32: break;
		case GcnOpcode::V_CMPS_NGT_F32: break;
		case GcnOpcode::V_CMPS_NLE_F32: break;
		case GcnOpcode::V_CMPS_NEQ_F32: break;
		case GcnOpcode::V_CMPS_NLT_F32: break;
		case GcnOpcode::V_CMPS_TRU_F32: break;
		case GcnOpcode::V_CMPS_T_F32: break;
		case GcnOpcode::V_CMPSX_F_F32: break;
		case GcnOpcode::V_CMPSX_LT_F32: break;
		case GcnOpcode::V_CMPSX_EQ_F32: break;
		case GcnOpcode::V_CMPSX_LE_F32: break;
		case GcnOpcode::V_CMPSX_GT_F32: break;
		case GcnOpcode::V_CMPSX_LG_F32: break;
		case GcnOpcode::V_CMPSX_GE_F32: break;
		case GcnOpcode::V_CMPSX_O_F32: break;
		case GcnOpcode::V_CMPSX_U_F32: break;
		case GcnOpcode::V_CMPSX_NGE_F32: break;
		case GcnOpcode::V_CMPSX_NLG_F32: break;
		case GcnOpcode::V_CMPSX_NGT_F32: break;
		case GcnOpcode::V_CMPSX_NLE_F32: break;
		case GcnOpcode::V_CMPSX_NEQ_F32: break;
		case GcnOpcode::V_CMPSX_NLT_F32: break;
		case GcnOpcode::V_CMPSX_TRU_F32: break;
		case GcnOpcode::V_CMPSX_T_F32: break;
		case GcnOpcode::V_CMPS_F_F64: break;
		case GcnOpcode::V_CMPS_LT_F64: break;
		case GcnOpcode::V_CMPS_EQ_F64: break;
		case GcnOpcode::V_CMPS_LE_F64: break;
		case GcnOpcode::V_CMPS_GT_F64: break;
		case GcnOpcode::V_CMPS_LG_F64: break;
		case GcnOpcode::V_CMPS_GE_F64: break;
		case GcnOpcode::V_CMPS_O_F64: break;
		case GcnOpcode::V_CMPS_U_F64: break;
		case GcnOpcode::V_CMPS_NGE_F64: break;
		case GcnOpcode::V_CMPS_NLG_F64: break;
		case GcnOpcode::V_CMPS_NGT_F64: break;
		case GcnOpcode::V_CMPS_NLE_F64: break;
		case GcnOpcode::V_CMPS_NEQ_F64: break;
		case GcnOpcode::V_CMPS_NLT_F64: break;
		case GcnOpcode::V_CMPS_TRU_F64: break;
		case GcnOpcode::V_CMPS_T_F64: break;
		case GcnOpcode::V_CMPSX_F_F64: break;
		case GcnOpcode::V_CMPSX_LT_F64: break;
		case GcnOpcode::V_CMPSX_EQ_F64: break;
		case GcnOpcode::V_CMPSX_LE_F64: break;
		case GcnOpcode::V_CMPSX_GT_F64: break;
		case GcnOpcode::V_CMPSX_LG_F64: break;
		case GcnOpcode::V_CMPSX_GE_F64: break;
		case GcnOpcode::V_CMPSX_O_F64: break;
		case GcnOpcode::V_CMPSX_U_F64: break;
		case GcnOpcode::V_CMPSX_NGE_F64: break;
		case GcnOpcode::V_CMPSX_NLG_F64: break;
		case GcnOpcode::V_CMPSX_NGT_F64: break;
		case GcnOpcode::V_CMPSX_NLE_F64: break;
		case GcnOpcode::V_CMPSX_NEQ_F64: break;
		case GcnOpcode::V_CMPSX_NLT_F64: break;
		case GcnOpcode::V_CMPSX_TRU_F64: break;
		case GcnOpcode::V_CMPSX_T_F64: break;
		case GcnOpcode::V_CMP_TRU_I32: break;
		case GcnOpcode::V_CMPX_F_I32: break;
		case GcnOpcode::V_CMPX_LG_I32: break;
		case GcnOpcode::V_CMPX_TRU_I32: break;
		case GcnOpcode::V_CMPX_T_I32: break;
		case GcnOpcode::V_CMPX_CLASS_F32: break;
		case GcnOpcode::V_CMP_F_I64: break;
		case GcnOpcode::V_CMP_LT_I64: break;
		case GcnOpcode::V_CMP_EQ_I64: break;
		case GcnOpcode::V_CMP_LE_I64: break;
		case GcnOpcode::V_CMP_GT_I64: break;
		case GcnOpcode::V_CMP_LG_I64: break;
		case GcnOpcode::V_CMP_NE_I64: break;
		case GcnOpcode::V_CMP_GE_I64: break;
		case GcnOpcode::V_CMP_TRU_I64: break;
		case GcnOpcode::V_CMP_T_I64: break;
		case GcnOpcode::V_CMP_CLASS_F64: break;
		case GcnOpcode::V_CMPX_F_I64: break;
		case GcnOpcode::V_CMPX_LT_I64: break;
		case GcnOpcode::V_CMPX_EQ_I64: break;
		case GcnOpcode::V_CMPX_LE_I64: break;
		case GcnOpcode::V_CMPX_GT_I64: break;
		case GcnOpcode::V_CMPX_LG_I64: break;
		case GcnOpcode::V_CMPX_NE_I64: break;
		case GcnOpcode::V_CMPX_GE_I64: break;
		case GcnOpcode::V_CMPX_TRU_I64: break;
		case GcnOpcode::V_CMPX_T_I64: break;
		case GcnOpcode::V_CMPX_CLASS_F64: break;
		case GcnOpcode::V_CMP_TRU_U32: break;
		case GcnOpcode::V_CMP_F_F16: break;
		case GcnOpcode::V_CMP_O_F16: break;
		case GcnOpcode::V_CMPX_F_U32: break;
		case GcnOpcode::V_CMPX_TRU_U32: break;
		case GcnOpcode::V_CMPX_T_U32: break;
		case GcnOpcode::V_CMP_F_U64: break;
		case GcnOpcode::V_CMP_LT_U64: break;
		case GcnOpcode::V_CMP_EQ_U64: break;
		case GcnOpcode::V_CMP_LE_U64: break;
		case GcnOpcode::V_CMP_GT_U64: break;
		case GcnOpcode::V_CMP_LG_U64: break;
		case GcnOpcode::V_CMP_GE_U64: break;
		case GcnOpcode::V_CMP_TRU_U64: break;
		case GcnOpcode::V_CMP_T_U64: break;
		case GcnOpcode::V_CMPX_F_U64: break;
		case GcnOpcode::V_CMPX_LT_U64: break;
		case GcnOpcode::V_CMPX_EQ_U64: break;
		case GcnOpcode::V_CMPX_LE_U64: break;
		case GcnOpcode::V_CMPX_GT_U64: break;
		case GcnOpcode::V_CMPX_LG_U64: break;
		case GcnOpcode::V_CMPX_NE_U64: break;
		case GcnOpcode::V_CMPX_GE_U64: break;
		case GcnOpcode::V_CMPX_TRU_U64: break;
		case GcnOpcode::V_CMPX_T_U64: break;
		case GcnOpcode::V_MAC_LEGACY_F32: break;
		case GcnOpcode::V_MUL_LEGACY_F32: break;
		case GcnOpcode::V_MUL_HI_I32_I24: break;
		case GcnOpcode::V_MUL_HI_U32_U24: break;
		case GcnOpcode::V_MIN_LEGACY_F32: break;
		case GcnOpcode::V_MAX_LEGACY_F32: break;
		case GcnOpcode::V_SUBB_U32: break;
		case GcnOpcode::V_SUBBREV_U32: break;
		case GcnOpcode::V_CVT_PKACCUM_U8_F32: break;
		case GcnOpcode::V_CVT_PK_I16_I32: break;
		case GcnOpcode::V_LDEXP_F16: break;
		case GcnOpcode::V_CVT_I32_F64: break;
		case GcnOpcode::V_CVT_F64_I32: break;
		case GcnOpcode::V_MOV_FED_B32: break;
		case GcnOpcode::V_CVT_F32_F64: break;
		case GcnOpcode::V_CVT_F64_F32: break;
		case GcnOpcode::V_CVT_U32_F64: break;
		case GcnOpcode::V_CVT_F64_U32: break;
		case GcnOpcode::V_TRUNC_F64: break;
		case GcnOpcode::V_CEIL_F64: break;
		case GcnOpcode::V_RNDNE_F64: break;
		case GcnOpcode::V_FLOOR_F64: break;
		case GcnOpcode::V_LOG_CLAMP_F32: break;
		case GcnOpcode::V_RCP_CLAMP_F32: break;
		case GcnOpcode::V_RCP_LEGACY_F32: break;
		case GcnOpcode::V_RCP_IFLAG_F32: break;
		case GcnOpcode::V_RSQ_CLAMP_F32: break;
		case GcnOpcode::V_RSQ_LEGACY_F32: break;
		case GcnOpcode::V_RCP_F64: break;
		case GcnOpcode::V_RCP_CLAMP_F64: break;
		case GcnOpcode::V_RSQ_F64: break;
		case GcnOpcode::V_RSQ_CLAMP_F64: break;
		case GcnOpcode::V_SQRT_F64: break;
		case GcnOpcode::V_FFBH_I32: break;
		case GcnOpcode::V_FREXP_EXP_I32_F64: break;
		case GcnOpcode::V_FREXP_MANT_F64: break;
		case GcnOpcode::V_FRACT_F64: break;
		case GcnOpcode::V_FREXP_EXP_I32_F32: break;
		case GcnOpcode::V_FREXP_MANT_F32: break;
		case GcnOpcode::V_CLREXCP: break;
		case GcnOpcode::V_MOVRELSD_B32: break;
		case GcnOpcode::V_LOG_LEGACY_F32: break;
		case GcnOpcode::V_EXP_LEGACY_F32: break;
		case GcnOpcode::V_MAD_LEGACY_F32: break;
		case GcnOpcode::V_FMA_F64: break;
		case GcnOpcode::V_LERP_U8: break;
		case GcnOpcode::V_ALIGNBYTE_B32: break;
		case GcnOpcode::V_MULLIT_F32: break;
		case GcnOpcode::V_SAD_U8: break;
		case GcnOpcode::V_SAD_HI_U8: break;
		case GcnOpcode::V_SAD_U16: break;
		case GcnOpcode::V_DIV_FIXUP_F32: break;
		case GcnOpcode::V_DIV_FIXUP_F64: break;
		case GcnOpcode::V_LSHL_B64: break;
		case GcnOpcode::V_LSHR_B64: break;
		case GcnOpcode::V_ASHR_I64: break;
		case GcnOpcode::V_ADD_F64: break;
		case GcnOpcode::V_MUL_F64: break;
		case GcnOpcode::V_MIN_F64: break;
		case GcnOpcode::V_MAX_F64: break;
		case GcnOpcode::V_LDEXP_F64: break;
		case GcnOpcode::V_DIV_SCALE_F32: break;
		case GcnOpcode::V_DIV_SCALE_F64: break;
		case GcnOpcode::V_DIV_FMAS_F32: break;
		case GcnOpcode::V_DIV_FMAS_F64: break;
		case GcnOpcode::V_MSAD_U8: break;
		case GcnOpcode::V_QSAD_U8: break;
		case GcnOpcode::V_QSAD_PK_U16_U8: break;
		case GcnOpcode::V_MQSAD_U8: break;
		case GcnOpcode::V_MQSAD_PK_U16_U8: break;
		case GcnOpcode::V_TRIG_PREOP_F64: break;
		case GcnOpcode::V_MQSAD_U32_U8: break;
		case GcnOpcode::V_MAD_I64_I32: break;
		case GcnOpcode::V_SUB_CO_U32: break;
		case GcnOpcode::V_MAD_F16: break;
		case GcnOpcode::V_MAD_I16: break;
		case GcnOpcode::S_MEMTIME: break;
		case GcnOpcode::S_DCACHE_INV: break;
		case GcnOpcode::DS_RSUB_U32: break;
		case GcnOpcode::DS_INC_U32: break;
		case GcnOpcode::DS_DEC_U32: break;
		case GcnOpcode::DS_MSKOR_B32: break;
		case GcnOpcode::DS_WRITE2ST64_B32: break;
		case GcnOpcode::DS_CMPST_B32: break;
		case GcnOpcode::DS_CMPST_F32: break;
		case GcnOpcode::DS_NOP: break;
		case GcnOpcode::DS_GWS_SEMA_RELEASE_ALL: break;
		case GcnOpcode::DS_GWS_INIT: break;
		case GcnOpcode::DS_GWS_SEMA_V: break;
		case GcnOpcode::DS_GWS_SEMA_BR: break;
		case GcnOpcode::DS_GWS_SEMA_P: break;
		case GcnOpcode::DS_GWS_BARRIER: break;
		case GcnOpcode::DS_WRITE_B8: break;
		case GcnOpcode::DS_WRITE_B16: break;
		case GcnOpcode::DS_RSUB_RTN_U32: break;
		case GcnOpcode::DS_INC_RTN_U32: break;
		case GcnOpcode::DS_DEC_RTN_U32: break;
		case GcnOpcode::DS_MSKOR_RTN_B32: break;
		case GcnOpcode::DS_WRXCHG2_RTN_B32: break;
		case GcnOpcode::DS_WRXCHG2ST64_RTN_B32: break;
		case GcnOpcode::DS_CMPST_RTN_B32: break;
		case GcnOpcode::DS_CMPST_RTN_F32: break;
		case GcnOpcode::DS_MIN_RTN_F32: break;
		case GcnOpcode::DS_MAX_RTN_F32: break;
		case GcnOpcode::DS_WRAP_RTN_B32: break;
		case GcnOpcode::DS_READ2ST64_B32: break;
		case GcnOpcode::DS_READ_I8: break;
		case GcnOpcode::DS_READ_U8: break;
		case GcnOpcode::DS_READ_I16: break;
		case GcnOpcode::DS_READ_U16: break;
		case GcnOpcode::DS_ORDERED_COUNT: break;
		case GcnOpcode::DS_ADD_U64: break;
		case GcnOpcode::DS_SUB_U64: break;
		case GcnOpcode::DS_RSUB_U64: break;
		case GcnOpcode::DS_INC_U64: break;
		case GcnOpcode::DS_DEC_U64: break;
		case GcnOpcode::DS_MIN_I64: break;
		case GcnOpcode::DS_MAX_I64: break;
		case GcnOpcode::DS_MIN_U64: break;
		case GcnOpcode::DS_MAX_U64: break;
		case GcnOpcode::DS_AND_B64: break;
		case GcnOpcode::DS_OR_B64: break;
		case GcnOpcode::DS_XOR_B64: break;
		case GcnOpcode::DS_MSKOR_B64: break;
		case GcnOpcode::DS_WRITE2ST64_B64: break;
		case GcnOpcode::DS_CMPST_B64: break;
		case GcnOpcode::DS_CMPST_F64: break;
		case GcnOpcode::DS_MIN_F64: break;
		case GcnOpcode::DS_MAX_F64: break;
		case GcnOpcode::DS_ADD_RTN_U64: break;
		case GcnOpcode::DS_SUB_RTN_U64: break;
		case GcnOpcode::DS_RSUB_RTN_U64: break;
		case GcnOpcode::DS_INC_RTN_U64: break;
		case GcnOpcode::DS_DEC_RTN_U64: break;
		case GcnOpcode::DS_MIN_RTN_I64: break;
		case GcnOpcode::DS_MAX_RTN_I64: break;
		case GcnOpcode::DS_MIN_RTN_U64: break;
		case GcnOpcode::DS_MAX_RTN_U64: break;
		case GcnOpcode::DS_AND_RTN_B64: break;
		case GcnOpcode::DS_OR_RTN_B64: break;
		case GcnOpcode::DS_XOR_RTN_B64: break;
		case GcnOpcode::DS_MSKOR_RTN_B64: break;
		case GcnOpcode::DS_WRXCHG_RTN_B64: break;
		case GcnOpcode::DS_WRXCHG2_RTN_B64: break;
		case GcnOpcode::DS_WRXCHG2ST64_RTN_B64: break;
		case GcnOpcode::DS_CMPST_RTN_B64: break;
		case GcnOpcode::DS_CMPST_RTN_F64: break;
		case GcnOpcode::DS_MIN_RTN_F64: break;
		case GcnOpcode::DS_MAX_RTN_F64: break;
		case GcnOpcode::DS_READ2ST64_B64: break;
		case GcnOpcode::DS_CONDXCHG32_RTN_B64: break;
		case GcnOpcode::DS_ADD_SRC2_U32: break;
		case GcnOpcode::DS_SUB_SRC2_U32: break;
		case GcnOpcode::DS_RSUB_SRC2_U32: break;
		case GcnOpcode::DS_INC_SRC2_U32: break;
		case GcnOpcode::DS_DEC_SRC2_U32: break;
		case GcnOpcode::DS_MIN_SRC2_I32: break;
		case GcnOpcode::DS_MAX_SRC2_I32: break;
		case GcnOpcode::DS_MIN_SRC2_U32: break;
		case GcnOpcode::DS_MAX_SRC2_U32: break;
		case GcnOpcode::DS_AND_SRC2_B32: break;
		case GcnOpcode::DS_OR_SRC2_B32: break;
		case GcnOpcode::DS_XOR_SRC2_B32: break;
		case GcnOpcode::DS_WRITE_SRC2_B32: break;
		case GcnOpcode::DS_MIN_SRC2_F32: break;
		case GcnOpcode::DS_MAX_SRC2_F32: break;
		case GcnOpcode::DS_ADD_SRC2_U64: break;
		case GcnOpcode::DS_SUB_SRC2_U64: break;
		case GcnOpcode::DS_RSUB_SRC2_U64: break;
		case GcnOpcode::DS_INC_SRC2_U64: break;
		case GcnOpcode::DS_DEC_SRC2_U64: break;
		case GcnOpcode::DS_MIN_SRC2_I64: break;
		case GcnOpcode::DS_MAX_SRC2_I64: break;
		case GcnOpcode::DS_MIN_SRC2_U64: break;
		case GcnOpcode::DS_MAX_SRC2_U64: break;
		case GcnOpcode::DS_AND_SRC2_B64: break;
		case GcnOpcode::DS_OR_SRC2_B64: break;
		case GcnOpcode::DS_XOR_SRC2_B64: break;
		case GcnOpcode::DS_WRITE_SRC2_B64: break;
		case GcnOpcode::DS_MIN_SRC2_F64: break;
		case GcnOpcode::DS_MAX_SRC2_F64: break;
		case GcnOpcode::DS_CONDXCHG32_RTN_B128: break;
		case GcnOpcode::BUFFER_ATOMIC_CMPSWAP: break;
		case GcnOpcode::BUFFER_ATOMIC_SMIN: break;
		case GcnOpcode::BUFFER_ATOMIC_UMIN: break;
		case GcnOpcode::BUFFER_ATOMIC_SMAX: break;
		case GcnOpcode::BUFFER_ATOMIC_UMAX: break;
		case GcnOpcode::BUFFER_ATOMIC_INC: break;
		case GcnOpcode::BUFFER_ATOMIC_DEC: break;
		case GcnOpcode::BUFFER_ATOMIC_FCMPSWAP: break;
		case GcnOpcode::BUFFER_ATOMIC_FMIN: break;
		case GcnOpcode::BUFFER_ATOMIC_FMAX: break;
		case GcnOpcode::BUFFER_ATOMIC_SWAP_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_CMPSWAP_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_ADD_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_SUB_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_SMIN_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_UMIN_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_SMAX_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_UMAX_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_AND_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_OR_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_XOR_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_INC_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_DEC_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_FCMPSWAP_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_FMIN_X2: break;
		case GcnOpcode::BUFFER_ATOMIC_FMAX_X2: break;
		case GcnOpcode::BUFFER_WBINVL1_SC: break;
		case GcnOpcode::BUFFER_WBINVL1: break;
		case GcnOpcode::TBUFFER_LOAD_FORMAT_X: break;
		case GcnOpcode::TBUFFER_LOAD_FORMAT_XY: break;
		case GcnOpcode::TBUFFER_LOAD_FORMAT_XYZ: break;
		case GcnOpcode::TBUFFER_LOAD_FORMAT_XYZW: break;
		case GcnOpcode::TBUFFER_STORE_FORMAT_X: break;
		case GcnOpcode::TBUFFER_STORE_FORMAT_XY: break;
		case GcnOpcode::TBUFFER_STORE_FORMAT_XYZ: break;
		case GcnOpcode::TBUFFER_STORE_FORMAT_XYZW: break;
		case GcnOpcode::IMAGE_LOAD_PCK: break;
		case GcnOpcode::IMAGE_LOAD_PCK_SGN: break;
		case GcnOpcode::IMAGE_LOAD_MIP_PCK: break;
		case GcnOpcode::IMAGE_LOAD_MIP_PCK_SGN: break;
		case GcnOpcode::IMAGE_STORE_PCK: break;
		case GcnOpcode::IMAGE_STORE_MIP_PCK: break;
		case GcnOpcode::IMAGE_ATOMIC_SWAP: break;
		case GcnOpcode::IMAGE_ATOMIC_CMPSWAP: break;
		case GcnOpcode::IMAGE_ATOMIC_SUB: break;
		case GcnOpcode::IMAGE_ATOMIC_SMIN: break;
		case GcnOpcode::IMAGE_ATOMIC_UMIN: break;
		case GcnOpcode::IMAGE_ATOMIC_SMAX: break;
		case GcnOpcode::IMAGE_ATOMIC_UMAX: break;
		case GcnOpcode::IMAGE_ATOMIC_INC: break;
		case GcnOpcode::IMAGE_ATOMIC_DEC: break;
		case GcnOpcode::IMAGE_ATOMIC_FCMPSWAP: break;
		case GcnOpcode::IMAGE_ATOMIC_FMIN: break;
		case GcnOpcode::IMAGE_ATOMIC_FMAX: break;
		case GcnOpcode::IMAGE_SAMPLE_CL: break;
		case GcnOpcode::IMAGE_SAMPLE_D: break;
		case GcnOpcode::IMAGE_SAMPLE_D_CL: break;
		case GcnOpcode::IMAGE_SAMPLE_L: break;
		case GcnOpcode::IMAGE_SAMPLE_B: break;
		case GcnOpcode::IMAGE_SAMPLE_B_CL: break;
		case GcnOpcode::IMAGE_SAMPLE_LZ: break;
		case GcnOpcode::IMAGE_SAMPLE_C: break;
		case GcnOpcode::IMAGE_SAMPLE_C_CL: break;
		case GcnOpcode::IMAGE_SAMPLE_C_D: break;
		case GcnOpcode::IMAGE_SAMPLE_C_D_CL: break;
		case GcnOpcode::IMAGE_SAMPLE_C_L: break;
		case GcnOpcode::IMAGE_SAMPLE_C_B: break;
		case GcnOpcode::IMAGE_SAMPLE_C_B_CL: break;
		case GcnOpcode::IMAGE_SAMPLE_C_LZ: break;
		case GcnOpcode::IMAGE_SAMPLE_O: break;
		case GcnOpcode::IMAGE_SAMPLE_CL_O: break;
		case GcnOpcode::IMAGE_SAMPLE_D_O: break;
		case GcnOpcode::IMAGE_SAMPLE_D_CL_O: break;
		case GcnOpcode::IMAGE_SAMPLE_L_O: break;
		case GcnOpcode::IMAGE_SAMPLE_B_O: break;
		case GcnOpcode::IMAGE_SAMPLE_B_CL_O: break;
		case GcnOpcode::IMAGE_SAMPLE_LZ_O: break;
		case GcnOpcode::IMAGE_SAMPLE_C_O: break;
		case GcnOpcode::IMAGE_SAMPLE_C_CL_O: break;
		case GcnOpcode::IMAGE_SAMPLE_C_D_O: break;
		case GcnOpcode::IMAGE_SAMPLE_C_D_CL_O: break;
		case GcnOpcode::IMAGE_SAMPLE_C_L_O: break;
		case GcnOpcode::IMAGE_SAMPLE_C_B_O: break;
		case GcnOpcode::IMAGE_SAMPLE_C_B_CL_O: break;
		case GcnOpcode::IMAGE_SAMPLE_C_LZ_O: break;
		case GcnOpcode::IMAGE_GATHER4: break;
		case GcnOpcode::IMAGE_GATHER4_CL: break;
		case GcnOpcode::IMAGE_GATHER4_L: break;
		case GcnOpcode::IMAGE_GATHER4_B: break;
		case GcnOpcode::IMAGE_GATHER4_B_CL: break;
		case GcnOpcode::IMAGE_GATHER4_C_CL: break;
		case GcnOpcode::IMAGE_GATHER4_C_L: break;
		case GcnOpcode::IMAGE_GATHER4_C_B: break;
		case GcnOpcode::IMAGE_GATHER4_C_B_CL: break;
		case GcnOpcode::IMAGE_GATHER4_O: break;
		case GcnOpcode::IMAGE_GATHER4_CL_O: break;
		case GcnOpcode::IMAGE_GATHER4_L_O: break;
		case GcnOpcode::IMAGE_GATHER4_B_O: break;
		case GcnOpcode::IMAGE_GATHER4_B_CL_O: break;
		case GcnOpcode::IMAGE_GATHER4_C_CL_O: break;
		case GcnOpcode::IMAGE_GATHER4_C_L_O: break;
		case GcnOpcode::IMAGE_GATHER4_C_B_O: break;
		case GcnOpcode::IMAGE_GATHER4_C_B_CL_O: break;
		case GcnOpcode::IMAGE_SAMPLE_CD: break;
		case GcnOpcode::IMAGE_SAMPLE_CD_CL: break;
		case GcnOpcode::IMAGE_SAMPLE_C_CD: break;
		case GcnOpcode::IMAGE_SAMPLE_C_CD_CL: break;
		case GcnOpcode::IMAGE_SAMPLE_CD_O: break;
		case GcnOpcode::IMAGE_SAMPLE_CD_CL_O: break;
		case GcnOpcode::IMAGE_SAMPLE_C_CD_O: break;
		case GcnOpcode::IMAGE_SAMPLE_C_CD_CL_O: break;
		case GcnOpcode::EXP: break;
		case GcnOpcode::V_MAD_MIX_F32: break;
		default: break;
	}
	return Opcode::Unsupported;
}


std::string GcnOpcodeName(GcnOpcode op) {
	const auto name = magic_enum::enum_name(op);
	return std::string(name.data(), name.length());
}

bool IsBranch(GcnOpcode op) {
	switch (op) {
		case GcnOpcode::S_BRANCH:
		case GcnOpcode::S_CBRANCH_SCC0:
		case GcnOpcode::S_CBRANCH_SCC1:
		case GcnOpcode::S_CBRANCH_VCCZ:
		case GcnOpcode::S_CBRANCH_VCCNZ:
		case GcnOpcode::S_CBRANCH_EXECZ:
		case GcnOpcode::S_CBRANCH_EXECNZ:
			return true;
		default:
			return false;
	}
}

} // namespace

bool DecodeGcnProgram(std::span<const uint32_t> code, Program& program,
                      std::string* error) {
	if (code.empty() || code.size() > UINT32_MAX / sizeof(uint32_t)) {
		if (error != nullptr) {
			*error = "invalid GCN shader decoder input";
		}
		return false;
	}

	program.instructions.clear();
	program.code = code;
	program.isa_family = ShaderRecompiler::IsaFamily::Gcn;

	Shader::Gcn::GcnDecodeContext ctx;
	Shader::Gcn::GcnCodeSlice slice(code.data(), code.data() + code.size());

	uint32_t word_index = 0;
	while (!slice.atEnd()) {
		const uint32_t pc = word_index * 4u;

		GcnInst ginst = ctx.decodeInstruction(slice);
		const uint32_t words = ginst.length / sizeof(uint32_t);
		if (words == 0) {
			if (error != nullptr) {
				*error = fmt::format("GCN decoder produced zero-length instruction at pc 0x{:08x}", pc);
			}
			return false;
		}

		Instruction inst{};
		inst.pc = pc;
		inst.word = code[word_index];
		inst.word_count = words;
		inst.raw_count = std::min<uint32_t>(words, MaxInstructionRawWords);
		for (uint32_t i = 0; i < inst.raw_count; i++) {
			inst.raw[i] = code[word_index + i];
		}
		inst.family = MapFamily(ginst.encoding);
		inst.opcode_id = static_cast<uint32_t>(ginst.opcode);

		const Opcode mapped = MapOpcode(ginst.opcode);
		if (mapped == Opcode::Unsupported) {
			inst.opcode = Opcode::Unsupported;
			inst.unsupported_reason =
			    fmt::format("GCN opcode {} (encoding {}) not yet mapped",
			                GcnOpcodeName(ginst.opcode), static_cast<uint32_t>(ginst.encoding));
		} else {
			inst.opcode = mapped;
		}

		// Operands. GcnInst holds up to GcnMaxSrcCount sources and up to
		// GcnMaxDstCount destinations; Kyty exposes dst/dst2/src0..src3.
		if (ginst.dst_count >= 1) inst.dst = MapOperand(ginst.dst[0]);
		if (ginst.dst_count >= 2) inst.dst2 = MapOperand(ginst.dst[1]);
		inst.src_count = ginst.src_count;
		if (ginst.src_count >= 1) inst.src0 = MapOperand(ginst.src[0]);
		if (ginst.src_count >= 2) inst.src1 = MapOperand(ginst.src[1]);
		if (ginst.src_count >= 3) inst.src2 = MapOperand(ginst.src[2]);
		if (ginst.src_count >= 4) inst.src3 = MapOperand(ginst.src[3]);

		// Branch target (SOPP relative branch).
		if (IsBranch(ginst.opcode)) {
			inst.branch_target = ginst.BranchTarget(pc);
			inst.branch_offset = static_cast<int32_t>(inst.branch_target) - static_cast<int32_t>(pc);
		}

		// Export metadata.
		if (ginst.encoding == GcnEncoding::EXP) {
			inst.exp.target = ginst.control.exp.target;
			inst.exp.en = ginst.control.exp.en;
			inst.exp.done = ginst.control.exp.done;
			inst.exp.compr = ginst.control.exp.compr;
			inst.exp.vm = ginst.control.exp.vm;
		}

		program.instructions.push_back(std::move(inst));
		word_index += words;

		// S_ENDPGM terminates the program unless a branch target points past it.
		if (ginst.opcode == GcnOpcode::S_ENDPGM && word_index >= code.size()) {
			return true;
		}
	}

	return true;
}

} // namespace Libs::Graphics::ShaderRecompiler::Decoder