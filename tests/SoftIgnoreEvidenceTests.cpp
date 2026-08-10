#include "graphics/guest_gpu/command_processor/pm4SoftIgnoreProbe.h"
#include "graphics/guest_gpu/pm4.h"
#include "libs/ngs2_pcm.h"
#include "loader/nullPageFaultLog.h"
#include "loader/x64InstructionEmulator.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace Pm4 = Libs::Graphics::Pm4;
using Libs::Graphics::Pm4SoftIgnore::ProbePacketStream;
using Libs::Graphics::Pm4SoftIgnore::FormatReport;
using Libs::Graphics::Pm4SoftIgnore::IsCxHandled;
using Loader::X64InstructionEmulator::EstimateNullPageSkipLength;
using Loader::X64InstructionEmulator::NullPageFaultFingerprint;
using Loader::X64InstructionEmulator::RecordNullPageFault;
using Loader::X64InstructionEmulator::ResetNullPageFaultLog;
using Loader::X64InstructionEmulator::CopyNullPageFaults;
using Loader::X64InstructionEmulator::NullPageFaultCount;
using Loader::X64InstructionEmulator::FormatNullPageFaultFingerprint;

static void Require(bool ok, const char* what) {
	if (!ok) {
		std::fprintf(stderr, "FAIL: %s\n", what);
		std::abort();
	}
}

static void PushSetContext(std::vector<uint32_t>& stream, uint32_t offset, uint32_t value) {
	stream.push_back(0xC0016900u);
	stream.push_back(offset);
	stream.push_back(value);
}

static void TestPm4DryRunSoftIgnore() {
	std::vector<uint32_t> stream;
	// Type-3 NOP (KYTY_PM4_LEN == 2 ⇒ header + 1 dword)
	stream.push_back(0xC0001000u);
	stream.push_back(0);
	// Type-3 SET_CONTEXT_REG poly-offset DB_FMT through BACK_OFFSET (len=8)
	stream.push_back(0xC0066900u);
	stream.push_back(Pm4::PA_SU_POLY_OFFSET_DB_FMT_CNTL);
	for (uint32_t i = 0; i < 6; ++i) {
		stream.push_back(0);
	}
	PushSetContext(stream, Pm4::CB_SHADER_MASK, 0x0F0F0F0Fu);
	PushSetContext(stream, Pm4::DB_SHADER_CONTROL, 0x100u);
	PushSetContext(stream, Pm4::SPI_BARYC_CNTL, 0);
	PushSetContext(stream, Pm4::SPI_PS_INPUT_ENA, 0);
	PushSetContext(stream, Pm4::SPI_INTERP_CONTROL_0, 0);
	PushSetContext(stream, Pm4::PA_SC_SHADER_CONTROL, 0);
	PushSetContext(stream, Pm4::SPI_SHADER_Z_FORMAT, 0);
	PushSetContext(stream, Pm4::DB_STENCILREFMASK_BF, 0);
	PushSetContext(stream, Pm4::VGT_MULTI_PRIM_IB_RESET_INDX, 0xFFFFFFFFu);
	PushSetContext(stream, Pm4::CB_BLEND_ALPHA, 0x3f800000u);
	// Type-3 SET_CONTEXT_REG at a likely-unhandled high CX offset (len=3)
	stream.push_back(0xC0016900u);
	stream.push_back(0x3F0u);
	stream.push_back(0xdeadbeefu);

	const auto report = ProbePacketStream(stream, false);
	Require(report.packets_parsed >= 12, "parsed packets");
	Require(report.regs_checked > 0, "checked regs");
	Require(IsCxHandled(Pm4::CB_SHADER_MASK), "CB_SHADER_MASK handled");
	Require(IsCxHandled(Pm4::DB_SHADER_CONTROL), "DB_SHADER_CONTROL handled");
	Require(IsCxHandled(Pm4::SPI_BARYC_CNTL), "SPI_BARYC_CNTL handled");
	Require(IsCxHandled(Pm4::SPI_INTERP_CONTROL_0), "SPI_INTERP_CONTROL_0 handled");
	Require(IsCxHandled(Pm4::DB_STENCILREFMASK_BF), "DB_STENCILREFMASK_BF handled");
	Require(IsCxHandled(Pm4::CB_BLEND_ALPHA), "CB_BLEND_ALPHA handled");
	bool soft_3f0 = false;
	for (const auto& hit: report.packet_soft_ignores) {
		if (hit.offset == 0x3F0u) {
			soft_3f0 = true;
		}
		Require(hit.offset != Pm4::CB_SHADER_MASK, "shader mask not soft-ignored");
		Require(hit.offset != Pm4::DB_SHADER_CONTROL, "db shader control not soft-ignored");
		Require(hit.offset != Pm4::SPI_BARYC_CNTL, "baryc not soft-ignored");
	}
	Require(soft_3f0, "unknown 0x3F0 soft-ignored");
	const auto text = FormatReport(report);
	Require(!text.empty(), "report text");
	std::fputs(text.c_str(), stdout);
}

static void TestNgs2UnsupportedWaveformPolicy() {
	using namespace Libs::Audio;
	Require(Ngs2Pcm::IsSupported(Ngs2Pcm::kWavePcmI16Little), "pcm supported");
	Require(!Ngs2Pcm::IsSupported(0x20u), "atrac9-like id unsupported without ABI");
	Require(!Ngs2Pcm::IsSupported(0x30u), "unknown waveform unsupported");
}

static void TestNullPageFingerprint() {
	ResetNullPageFaultLog();
	Require(NullPageFaultCount() == 0, "reset clears count");

	NullPageFaultFingerprint fault {};
	fault.rip          = 0x140001234;
	fault.access_vaddr = 0x8;
	fault.skip_length  = 2;
	fault.opcode[0]    = 0x89;
	fault.opcode[1]    = 0x08;
	fault.opcode_len   = 2;
	RecordNullPageFault(fault);
	Require(NullPageFaultCount() == 1, "fault counted");
	NullPageFaultFingerprint out[4] {};
	const size_t n = CopyNullPageFaults(out, 4);
	Require(n == 1, "fault copied");
	Require(out[0].rip == fault.rip, "rip match");
	Require(EstimateNullPageSkipLength(fault.opcode, true) == 2, "len decode");
	const auto text = FormatNullPageFaultFingerprint(fault);
	Require(text.find("140001234") != std::string::npos, "format rip");
	Require(text.find("8908") != std::string::npos, "format opcode");

	// Ring wrap: capacity is 64; overfill and confirm oldest dropped.
	for (uint32_t i = 0; i < 70; ++i) {
		NullPageFaultFingerprint f {};
		f.rip          = 0x1000ull + i;
		f.access_vaddr = i;
		f.skip_length  = 1;
		f.opcode[0]    = 0x90;
		f.opcode_len   = 1;
		RecordNullPageFault(f);
	}
	Require(NullPageFaultCount() == 71, "total includes wrap");
	NullPageFaultFingerprint ring[64] {};
	const size_t copied = CopyNullPageFaults(ring, 64);
	Require(copied == 64, "ring capacity");
	Require(ring[0].rip == 0x1000ull + 6, "oldest after wrap");
	Require(ring[63].rip == 0x1000ull + 69, "newest after wrap");
}

int main() {
	TestPm4DryRunSoftIgnore();
	TestNgs2UnsupportedWaveformPolicy();
	TestNullPageFingerprint();
	return 0;
}
