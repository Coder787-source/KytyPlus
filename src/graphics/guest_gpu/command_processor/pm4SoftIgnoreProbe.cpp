#include "graphics/guest_gpu/command_processor/pm4SoftIgnoreProbe.h"

#include "graphics/guest_gpu/command_processor/pm4Dispatch.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <set>
#include <sstream>

namespace Libs::Graphics::Pm4SoftIgnore {

namespace {

std::atomic_bool g_dispatch_ready {false};

} // namespace

void EnsureDispatchReady() {
	// Direct dispatch tables are constinit. Indirect tables are only needed for
	// live CP execution; dry-run soft-ignore coverage uses the direct tables.
	g_dispatch_ready.store(true, std::memory_order_release);
}

bool IsCxHandled(uint32_t offset) noexcept {
	return offset < Pm4::CX_NUM && g_hw_ctx_func[offset] != nullptr;
}

bool IsShHandled(uint32_t offset) noexcept {
	return offset < Pm4::SH_NUM && g_hw_sh_func[offset] != nullptr;
}

bool IsUcHandled(uint32_t offset) noexcept {
	return offset < Pm4::UC_NUM && g_hw_uc_func[offset] != nullptr;
}

ProbeReport ProbePacketStream(std::span<const uint32_t> dwords, bool include_full_table_gaps) {
	EnsureDispatchReady();
	ProbeReport report {};
	std::set<uint32_t> soft_cx;
	std::set<uint32_t> soft_sh;
	std::set<uint32_t> soft_uc;
	std::set<uint32_t> touched_uc;

	size_t i = 0;
	while (i < dwords.size()) {
		const uint32_t header = dwords[i];
		const uint32_t type   = (header >> 30) & 0x3u;
		if (type == 2) {
			++i;
			continue;
		}
		if (type != 3) {
			break;
		}
		const uint32_t len = KYTY_PM4_LEN(header);
		if (len < 2 || i + len > dwords.size()) {
			break;
		}
		++report.packets_parsed;
		const uint32_t opcode = (header >> 8) & 0xffu;
		const uint32_t* body  = dwords.data() + i + 1;
		const uint32_t body_n = len - 1;

		auto note = [&](Bank bank, uint32_t offset, bool handled) {
			++report.regs_checked;
			if (handled) {
				return;
			}
			Hit hit {bank, offset};
			report.packet_soft_ignores.push_back(hit);
			switch (bank) {
				case Bank::Cx: soft_cx.insert(offset); break;
				case Bank::Sh: soft_sh.insert(offset); break;
				case Bank::Uc: soft_uc.insert(offset); break;
			}
		};

		if (opcode == Pm4::IT_SET_CONTEXT_REG && body_n >= 1) {
			const uint32_t start = body[0] & (Pm4::CX_NUM - 1u);
			for (uint32_t n = 0; n + 1 < body_n; ++n) {
				const uint32_t offset = (start + n) & (Pm4::CX_NUM - 1u);
				note(Bank::Cx, offset, IsCxHandled(offset));
			}
		} else if (opcode == Pm4::IT_SET_SH_REG && body_n >= 1) {
			const uint32_t start = body[0] & (Pm4::SH_NUM - 1u);
			for (uint32_t n = 0; n + 1 < body_n; ++n) {
				const uint32_t offset = (start + n) & (Pm4::SH_NUM - 1u);
				note(Bank::Sh, offset, IsShHandled(offset));
			}
		} else if ((opcode == Pm4::IT_SET_UCONFIG_REG || opcode == Pm4::IT_SET_UCONFIG_REG_INDEX) &&
		           body_n >= 1) {
			const uint32_t start = body[0] & (Pm4::UC_NUM - 1u);
			for (uint32_t n = 0; n + 1 < body_n; ++n) {
				const uint32_t offset = (start + n) & (Pm4::UC_NUM - 1u);
				touched_uc.insert(offset);
				note(Bank::Uc, offset, IsUcHandled(offset));
			}
		}
		i += len;
	}

	if (include_full_table_gaps) {
		for (uint32_t offset = 0; offset < Pm4::CX_NUM; ++offset) {
			if (!IsCxHandled(offset)) {
				report.uncovered_cx.push_back({Bank::Cx, offset});
			}
		}
		for (uint32_t offset = 0; offset < Pm4::SH_NUM; ++offset) {
			if (!IsShHandled(offset)) {
				report.uncovered_sh.push_back({Bank::Sh, offset});
			}
		}
	}
	for (const auto offset: touched_uc) {
		if (!IsUcHandled(offset)) {
			report.uncovered_uc_touched.push_back({Bank::Uc, offset});
		}
	}

	std::sort(report.packet_soft_ignores.begin(), report.packet_soft_ignores.end(),
	          [](const Hit& a, const Hit& b) {
		          if (a.bank != b.bank) {
			          return static_cast<uint8_t>(a.bank) < static_cast<uint8_t>(b.bank);
		          }
		          return a.offset < b.offset;
	          });
	report.packet_soft_ignores.erase(
	    std::unique(report.packet_soft_ignores.begin(), report.packet_soft_ignores.end(),
	                [](const Hit& a, const Hit& b) {
		                return a.bank == b.bank && a.offset == b.offset;
	                }),
	    report.packet_soft_ignores.end());
	return report;
}

std::string FormatReport(const ProbeReport& report) {
	std::ostringstream out;
	out << "pm4 soft-ignore probe: packets=" << report.packets_parsed
	    << " regs_checked=" << report.regs_checked
	    << " unique_soft_ignores=" << report.packet_soft_ignores.size() << "\n";
	for (const auto& hit: report.packet_soft_ignores) {
		const char* bank = hit.bank == Bank::Cx ? "CX" : hit.bank == Bank::Sh ? "SH" : "UC";
		char        line[96];
		std::snprintf(line, sizeof(line), "  soft-ignore %s offset=0x%04x\n", bank, hit.offset);
		out << line;
	}
	if (!report.uncovered_cx.empty()) {
		out << "  uncovered CX handlers=" << report.uncovered_cx.size() << "\n";
	}
	if (!report.uncovered_sh.empty()) {
		out << "  uncovered SH handlers=" << report.uncovered_sh.size() << "\n";
	}
	if (!report.uncovered_uc_touched.empty()) {
		out << "  uncovered touched UC=" << report.uncovered_uc_touched.size() << "\n";
	}
	return out.str();
}

} // namespace Libs::Graphics::Pm4SoftIgnore
