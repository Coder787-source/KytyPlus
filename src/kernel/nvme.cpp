#include "kernel/nvme.h"
#include "common/assert.h"
#include "common/file.h"
#include "common/logging/log.h"
#include "common/stringUtils.h"
#include "common/threads.h"
#include "kernel/memory.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace Libs::LibKernel::Nvme {

// Removed the problematic macro causing the "type specifier required" error
// LIB_NAME("libkernel", "libkernel");

static NvmeControllerState   g_ctrl {};
static Common::File          g_backing_file;
static Common::Mutex         g_ctrl_mutex;
static Common::Mutex         g_backing_mutex;
static NvmeInterruptCallback g_interrupt_callback = nullptr;
static bool                  g_initialized         = false;

// Forward declarations for mutual call dependencies
static void ProcessAdminCommand(const NvmeSQEntry* cmd);
static void ProcessIOCommand(uint16_t sq_id, const NvmeSQEntry* cmd);
static void RingSQDoorbell(uint16_t sq_id, uint16_t new_tail);
static void RingSQDoorbellInternal(uint16_t sq_id, uint16_t new_tail);
static void SignalInterrupt(uint16_t vector);
static bool ProcessPrpTransfer(uint64_t prp1, uint64_t prp2, uint64_t data_size,
                               uint8_t* local_buffer, bool write_to_guest);

static bool ReadPhysicalMemory(uint64_t phys_addr, void* buffer, uint64_t size) {
	if (phys_addr >= 13824ull * 1024ull * 1024ull) return false;
	return Memory::TryReadBacking(phys_addr, buffer, size);
}

static bool WritePhysicalMemory(uint64_t phys_addr, const void* buffer, uint64_t size) {
	if (phys_addr >= 13824ull * 1024ull * 1024ull) return false;
	return Memory::TryWriteBacking(phys_addr, buffer, size);
}

// =============================================================================
// PRP Transfer Engine
// =============================================================================

static bool ProcessPrpTransfer(uint64_t prp1, uint64_t prp2, uint64_t data_size,
                               uint8_t* local_buffer, bool write_to_guest) {
	if (data_size == 0) return true;

	auto xfer = [write_to_guest](uint64_t phys, uint8_t* buf, uint64_t sz) -> bool {
		return write_to_guest ? WritePhysicalMemory(phys, buf, sz)
		                      : ReadPhysicalMemory(phys, buf, sz);
	};

	uint64_t remaining = data_size;
	uint64_t offset    = 0;

	uint64_t prp1_off  = prp1 & (NVME_PRP_PAGE_SIZE - 1);
	uint64_t prp1_page = prp1 & ~(static_cast<uint64_t>(NVME_PRP_PAGE_SIZE) - 1);
	uint64_t prp1_sz   = std::min(remaining, NVME_PRP_PAGE_SIZE - prp1_off);

	if (!xfer(prp1_page + prp1_off, local_buffer + offset, prp1_sz)) return false;
	offset += prp1_sz;
	remaining -= prp1_sz;
	if (remaining == 0) return true;

	if (data_size <= NVME_PRP_PAGE_SIZE) {
		uint64_t prp2_off  = prp2 & (NVME_PRP_PAGE_SIZE - 1);
		uint64_t prp2_page = prp2 & ~(static_cast<uint64_t>(NVME_PRP_PAGE_SIZE) - 1);
		uint64_t prp2_sz   = std::min(remaining, NVME_PRP_PAGE_SIZE - prp2_off);
		return xfer(prp2_page + prp2_off, local_buffer + offset, prp2_sz);
	}

	constexpr uint32_t ENTRIES = NVME_PRP_PAGE_SIZE / sizeof(uint64_t);
	std::vector<uint64_t> prp_list(ENTRIES);

	while (remaining > 0 && prp2 != 0) {
		if (!ReadPhysicalMemory(prp2, prp_list.data(), NVME_PRP_PAGE_SIZE)) return false;
		for (uint32_t i = 0; i < ENTRIES && remaining > 0; i++) {
			if (prp_list[i] == 0) break;
			uint64_t e_off  = prp_list[i] & (NVME_PRP_PAGE_SIZE - 1);
			uint64_t e_page = prp_list[i] & ~(static_cast<uint64_t>(NVME_PRP_PAGE_SIZE) - 1);
			uint64_t e_sz   = std::min(remaining, NVME_PRP_PAGE_SIZE - e_off);
			if (!xfer(e_page + e_off, local_buffer + offset, e_sz)) return false;
			offset += e_sz;
			remaining -= e_sz;
		}
		prp2 = prp_list[ENTRIES - 1];
	}
	return remaining == 0;
}

// =============================================================================
// Completion Posting
// =============================================================================

static void SignalInterrupt(uint16_t vector) {
	if (g_interrupt_callback) g_interrupt_callback(vector);
}

static void CompleteCommand(NvmeQueuePair* sq, NvmeQueuePair* cq,
                            uint16_t command_id, uint16_t status, uint32_t cs = 0) {
	if (cq->base_addr == 0 || cq->size == 0) return;

	NvmeCQEntry cqe {};
	cqe.command_specific = cs;
	cqe.sq_head          = sq->head;
	cqe.sq_id            = (cq == &g_ctrl.admin_cq) ? 0
	                        : static_cast<uint16_t>(cq - &g_ctrl.io_cq[0] + 1);
	cqe.command_id       = command_id;
	cqe.status_field     = status;
	cqe.phase            = cq->phase ? 1u : 0u;

	uint64_t addr = cq->base_addr + static_cast<uint64_t>(cq->tail) * 16;
	WritePhysicalMemory(addr, &cqe, 16);
	cq->tail = static_cast<uint16_t>((cq->tail + 1) % cq->size);

	if (cq->irq_enabled && (g_ctrl.intms & (1u << cq->irq_vector)) == 0) {
		SignalInterrupt(cq->irq_vector);
	}
}

// =============================================================================
// MMIO Read
// =============================================================================

bool HandleMMIORead(uint32_t offset, uint32_t size, uint64_t* value_out) {
	if (!value_out || offset >= NVME_MMIO_BAR_SIZE) return false;
	Common::LockGuard lock(g_ctrl_mutex);
	uint64_t val = 0;

	switch (offset & ~3u) {
		case 0x0000:
			val = (static_cast<uint64_t>(NVME_CAP_MQES))
			    | (static_cast<uint64_t>(NVME_CAP_CSS_NVM) << 37ull)
			    | (static_cast<uint64_t>(NVME_CAP_DSTRD) << 32ull)
			    | (static_cast<uint64_t>(NVME_CAP_TO) << 24ull);
			break;
		case 0x0008: val = (NVME_VS_MAJOR << 16) | (NVME_VS_MINOR << 8) | NVME_VS_TER; break;
		case 0x000C: val = g_ctrl.intms; break;
		case 0x0014: val = g_ctrl.cc; break;
		case 0x001C:
			if (g_ctrl.controller_enabled) val |= CSTS_RDY;
			if (g_ctrl.shutdown_complete)  val |= CSTS_SHST_MASK;
			break;
		case 0x0024: val = g_ctrl.aqa; break;
		case 0x0028: val = static_cast<uint32_t>(g_ctrl.asq & 0xFFFFFFFFull); break;
		case 0x0030: val = static_cast<uint32_t>(g_ctrl.acq & 0xFFFFFFFFull); break;
		default: break;
	}
	*value_out = val;
	return true;
}

// =============================================================================
// MMIO Write
// =============================================================================

bool HandleMMIOWrite(uint32_t offset, uint32_t size, uint64_t value) {
	if (offset >= NVME_MMIO_BAR_SIZE) return false;
	Common::LockGuard lock(g_ctrl_mutex);

	constexpr uint32_t DB_BASE = 0x1000;
	if (offset >= DB_BASE) {
		uint32_t db_idx  = (offset - DB_BASE) / 4;
		bool     is_cq   = (db_idx % 2) == 1;
		uint16_t qid     = static_cast<uint16_t>(db_idx / 2);
		uint16_t db_val  = static_cast<uint16_t>(value & 0xFFFF);
	if (is_cq) {
		NvmeQueuePair* cq = (qid == 0) ? &g_ctrl.admin_cq
					  : (qid - 1 < g_ctrl.io_cq.size() ? &g_ctrl.io_cq[qid - 1] : nullptr);
		if (cq) cq->head = db_val;
	} else {
		// Since we are under LockGuard, we must release before calling RingSQDoorbell 
		// which acquires the same mutex to avoid deadlock.
		// In this codebase, LockGuard is likely a wrapper around std::lock_guard 
		// which does not have .Unlock(). 
		// We must use a scope-based lock or a unique_lock.
		// However, a better fix is to call a private internal process method 
		// that assumes lock is already held.
		RingSQDoorbellInternal(qid, db_val);
	}
		return true;
	}

	uint32_t v32 = static_cast<uint32_t>(value);
	switch (offset & ~3u) {
		case 0x000C: g_ctrl.intms |= v32; break;
		case 0x0010: g_ctrl.intms &= ~v32; break;
		case 0x0014: {
			uint32_t shn = (v32 & CC_SHN_MASK) >> CC_SHN_SHIFT;
			bool     en  = (v32 & CC_ENABLE) != 0;
			g_ctrl.cc    = v32;
			if (en && !g_ctrl.controller_enabled) {
				g_ctrl.controller_enabled = true;
				g_ctrl.shutdown_complete  = false;
				uint32_t aqa = g_ctrl.aqa;
				g_ctrl.admin_sq.size = static_cast<uint16_t>((aqa & 0xFFF) + 1);
				g_ctrl.admin_sq.entry_size = 64;
				g_ctrl.admin_sq.head = 0;
				g_ctrl.admin_sq.tail = 0;
				g_ctrl.admin_sq.base_addr = g_ctrl.asq;
				g_ctrl.admin_cq.size = static_cast<uint16_t>(((aqa >> 16) & 0xFFF) + 1);
				g_ctrl.admin_cq.entry_size = 16;
				g_ctrl.admin_cq.head = 0;
				g_ctrl.admin_cq.tail = 0;
				g_ctrl.admin_cq.base_addr = g_ctrl.acq;
				g_ctrl.admin_cq.phase = 1;
				g_ctrl.admin_cq.irq_enabled = true;
				LOGF("\tNVMe controller enabled\n");
			} else if (!en && g_ctrl.controller_enabled) {
				if (shn == 1 || shn == 2) {
					g_ctrl.shutdown_complete = true;
					Common::LockGuard bl(g_backing_mutex);
					if (!g_backing_file.IsInvalid()) g_backing_file.Flush();
				}
				g_ctrl.controller_enabled = false;
				LOGF("\tNVMe controller disabled\n");
			}
			break;
		}
		case 0x0020:
			g_ctrl.controller_enabled = false;
			g_ctrl.shutdown_complete  = false;
			g_ctrl.io_sq.clear();
			g_ctrl.io_cq.clear();
			break;
		case 0x0024: g_ctrl.aqa = v32; break;
		case 0x0028: g_ctrl.asq = (g_ctrl.asq & 0xFFFFFFFF00000000ull) | v32; break;
		case 0x002C: g_ctrl.asq = (g_ctrl.asq & 0xFFFFFFFFull) | (static_cast<uint64_t>(v32) << 32); break;
		case 0x0030: g_ctrl.acq = (g_ctrl.acq & 0xFFFFFFFF00000000ull) | v32; break;
		case 0x0034: g_ctrl.acq = (g_ctrl.acq & 0xFFFFFFFFull) | (static_cast<uint64_t>(v32) << 32); break;
		default: break;
	}
	return true;
}

// =============================================================================
// Doorbell Handler
// =============================================================================

void RingSQDoorbell(uint16_t sq_id, uint16_t new_tail) {
	Common::LockGuard lock(g_ctrl_mutex);
	RingSQDoorbellInternal(sq_id, new_tail);
}

void RingSQDoorbellInternal(uint16_t sq_id, uint16_t new_tail) {
	if (!g_ctrl.controller_enabled) return;

	NvmeQueuePair* sq = (sq_id == 0) ? &g_ctrl.admin_sq
	                 : (sq_id - 1 < g_ctrl.io_sq.size() ? &g_ctrl.io_sq[sq_id - 1] : nullptr);
	if (!sq || new_tail >= sq->size) return;

	sq->tail = new_tail;

	while (sq->head != sq->tail) {
		uint64_t addr = sq->base_addr + static_cast<uint64_t>(sq->head) * sq->entry_size;
		NvmeSQEntry cmd {};
		if (!ReadPhysicalMemory(addr, &cmd, sizeof(cmd))) break;

		if (sq_id == 0) {
			ProcessAdminCommand(&cmd);
		} else {
			ProcessIOCommand(sq_id, &cmd);
		}
		sq->head = static_cast<uint16_t>((sq->head + 1) % sq->size);
	}
}

// =============================================================================
// Forward declarations for command handlers
// =============================================================================

static void ProcessAdminCommand(const NvmeSQEntry* cmd);
static void ProcessIOCommand(uint16_t sq_id, const NvmeSQEntry* cmd);

// =============================================================================
// Admin Command Dispatcher
// =============================================================================

static void ProcessAdminCommand(const NvmeSQEntry* cmd) {
	switch (static_cast<NvmeAdminOpcode>(cmd->opcode)) {
		case NvmeAdminOpcode::DeleteIOSQ:
		case NvmeAdminOpcode::DeleteIOCQ: {
			uint16_t qid = static_cast<uint16_t>(cmd->cdw10 & 0xFFFF);
			if (qid == 0 || qid > g_ctrl.io_sq.size()) {
				CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0x000E);
				break;
			}
			if (static_cast<NvmeAdminOpcode>(cmd->opcode) == NvmeAdminOpcode::DeleteIOSQ)
				g_ctrl.io_sq[qid - 1] = {};
			else
				g_ctrl.io_cq[qid - 1] = {};
			CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0);
			break;
		}
		case NvmeAdminOpcode::CreateIOSQ: {
			uint16_t qid   = static_cast<uint16_t>(cmd->cdw10 & 0xFFFF);
			uint16_t qsize = static_cast<uint16_t>((cmd->cdw10 >> 16) & 0xFFFF) + 1;
			uint16_t cqid  = static_cast<uint16_t>((cmd->cdw[1] >> 16) & 0xFFFF);
			if (qid == 0 || qid > g_ctrl.io_sq.size() ||
			    cqid == 0 || cqid > g_ctrl.io_cq.size() ||
			    g_ctrl.io_cq[cqid - 1].size == 0) {
				CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0x000E);
				break;
			}
			auto& sq = g_ctrl.io_sq[qid - 1];
			if (sq.size != 0) {
				CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0x0003);
				break;
			}
			sq.base_addr = cmd->prp1;
			sq.size      = qsize;
			sq.entry_size = 64;
			sq.head = 0;
			sq.tail = 0;
			CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0);
			break;
		}
		case NvmeAdminOpcode::CreateIOCQ: {
			uint16_t qid  = static_cast<uint16_t>(cmd->cdw10 & 0xFFFF);
			uint16_t qsize= static_cast<uint16_t>((cmd->cdw10 >> 16) & 0xFFFF) + 1;
			uint16_t irq_vec = static_cast<uint16_t>((cmd->cdw[1] >> 16) & 0xFFFF);
			bool     irq_en  = (cmd->cdw[1] & 0x0001) != 0;
			if (qid == 0 || qid > g_ctrl.io_cq.size()) {
				CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0x000E);
				break;
			}
			auto& cq = g_ctrl.io_cq[qid - 1];
			if (cq.size != 0) {
				CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0x0003);
				break;
			}
			cq.base_addr   = cmd->prp1;
			cq.size        = qsize;
			cq.entry_size  = 16;
			cq.head = 0;
			cq.tail = 0;
			cq.phase = 1;
			cq.irq_vector = irq_vec;
			cq.irq_enabled= irq_en;
			CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0);
			break;
		}
		case NvmeAdminOpcode::Identify: {
			uint8_t cns = static_cast<uint8_t>(cmd->cdw10 & 0xFF);
			std::vector<uint8_t> data(4096, 0);
			bool ok = true;
			switch (cns) {
				case 0x00: {
					auto* ns = reinterpret_cast<NvmeIdentifyNamespace*>(data.data());
					ns->nsze = g_ctrl.namespace_lbas;
					ns->ncap = g_ctrl.namespace_lbas;
					ns->nuse = g_ctrl.namespace_lbas;
					ns->nlbaf = 1;
					ns->lbaf[0][0] = 9;
					break;
				}
				case 0x01: {
					auto* id = reinterpret_cast<NvmeIdentifyController*>(data.data());
					id->vid = 0x1987; id->ssvid = 0x1987;
					std::memcpy(id->sn, "PS5EMU_NVME123456789", 21);
					std::memcpy(id->mn, "KytyPS5 NVMe Emulated Controller v1.0", 37);
					std::memcpy(id->fr, "1.4.0\0\0\0", 8);
					id->mdts = 5; id->cntlid = 1;
					id->ver = (NVME_VS_MAJOR << 16) | (NVME_VS_MINOR << 8) | NVME_VS_TER;
					id->oacs = (1u<<1)|(1u<<3)|(1u<<8);
					id->acl = 4; id->aerl = 4; id->frmw = (1u<<1);
					id->lpa = (1u<<0); id->elpe = 63; id->npss = 1;
					id->wctemp = 0x0157; id->cctemp = 0x0157;
					id->sqes = 0x66; id->cqes = 0x44; id->maxcmd = 64; id->nn = 1;
					id->oncs = (1u<<0)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<6);
					id->vwc = 1; id->sgls[0] = 0; id->mnan = 1;
					std::memcpy(id->subnqn,
					           "nqn.2024-07.com.kyty:emulated-nvme-controller", 46);
					id->psd[0] = 0x09; id->psd[4] = 0x64;
					break;
				}
				case 0x02: case 0x10: {
					auto* list = reinterpret_cast<NvmeNamespaceList*>(data.data());
					list->nsid[0] = NVME_DEFAULT_NSID;
					break;
				}
				default:
					CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0x0002);
					ok = false;
					break;
			}
			if (ok) {
				if (!ProcessPrpTransfer(cmd->prp1, cmd->prp2, 4096, data.data(), true))
					CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0x4002);
				else
					CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0);
			}
			break;
		}
		case NvmeAdminOpcode::SetFeatures: {
			uint8_t fid = static_cast<uint8_t>(cmd->cdw10 & 0xFF);
			uint32_t val = cmd->cdw[1], result = val;
			if (fid == 0x07) {
				uint16_t num_sq = std::min(static_cast<uint16_t>(val & 0xFFFF),
				                           static_cast<uint16_t>(NVME_MAX_IO_QUEUES));
				uint16_t num_cq = std::min(static_cast<uint16_t>((val>>16) & 0xFFFF),
				                           static_cast<uint16_t>(NVME_MAX_IO_QUEUES));
				uint16_t n = std::min(num_sq, num_cq);
				g_ctrl.io_sq.resize(n);
				g_ctrl.io_cq.resize(n);
				for (uint16_t i = 0; i < n; i++) {
					g_ctrl.io_sq[i] = {}; g_ctrl.io_cq[i] = {};
				}
				result = (static_cast<uint32_t>(n) << 16) | n;
			}
			CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0, result);
			break;
		}
		case NvmeAdminOpcode::GetFeatures: {
			uint8_t fid = static_cast<uint8_t>(cmd->cdw10 & 0xFF);
			uint32_t result = 0;
			if (fid == 0x07)
				result = (static_cast<uint32_t>(g_ctrl.io_sq.size()) << 16)
				       | static_cast<uint32_t>(g_ctrl.io_sq.size());
			else if (fid == 0x06) result = 1;
			CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0, result);
			break;
		}
		case NvmeAdminOpcode::GetLogPage: {
			std::vector<uint8_t> log(4096, 0);
			uint64_t numd = (static_cast<uint64_t>(cmd->cdw10 & 0x0FFF) + 1) * 4;
			uint64_t xfer = std::min(numd, static_cast<uint64_t>(4096));
			if (xfer > 0) ProcessPrpTransfer(cmd->prp1, cmd->prp2, xfer, log.data(), true);
			CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0);
			break;
		}
		case NvmeAdminOpcode::AsyncEventRequest:
		case NvmeAdminOpcode::Abort:
		case NvmeAdminOpcode::KeepAlive:
			CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0);
			break;
		default:
			CompleteCommand(&g_ctrl.admin_sq, &g_ctrl.admin_cq, cmd->command_id, 0x0002);
			break;
	}
}

// =============================================================================
// I/O Command Dispatcher
// =============================================================================

static void ProcessIOCommand(uint16_t sq_id, const NvmeSQEntry* cmd) {
	auto* sq = &g_ctrl.io_sq[sq_id - 1];
	auto* cq = &g_ctrl.io_cq[sq_id - 1];

	switch (static_cast<NvmeNvmOpcode>(cmd->opcode)) {
		case NvmeNvmOpcode::Flush: {
			Common::LockGuard bl(g_backing_mutex);
			if (!g_backing_file.IsInvalid()) g_backing_file.Flush();
			CompleteCommand(sq, cq, cmd->command_id, 0);
			break;
		}
		case NvmeNvmOpcode::Read: {
			uint32_t nlb    = cmd->nlb + 1;
			uint64_t size   = static_cast<uint64_t>(nlb) * NVME_SECTOR_SIZE;
			uint64_t foff   = cmd->slba * NVME_SECTOR_SIZE;
			std::vector<uint8_t> buf(size);
			{
				Common::LockGuard bl(g_backing_mutex);
				if (!g_backing_file.IsInvalid()) {
					g_backing_file.Seek(foff);
					uint32_t rd = 0;
					g_backing_file.Read(buf.data(), static_cast<uint32_t>(size), &rd);
					if (rd < size) std::memset(buf.data() + rd, 0, size - rd);
				} else {
					std::memset(buf.data(), 0, size);
				}
			}
			if (!ProcessPrpTransfer(cmd->prp1, cmd->prp2, size, buf.data(), true))
				CompleteCommand(sq, cq, cmd->command_id, 0x4002);
			else
				CompleteCommand(sq, cq, cmd->command_id, 0);
			break;
		}
		case NvmeNvmOpcode::Write: {
			uint32_t nlb    = cmd->nlb + 1;
			uint64_t size   = static_cast<uint64_t>(nlb) * NVME_SECTOR_SIZE;
			uint64_t foff   = cmd->slba * NVME_SECTOR_SIZE;
			std::vector<uint8_t> buf(size);
			if (!ProcessPrpTransfer(cmd->prp1, cmd->prp2, size, buf.data(), false)) {
				CompleteCommand(sq, cq, cmd->command_id, 0x4002);
				break;
			}
			{
				Common::LockGuard bl(g_backing_mutex);
				if (!g_backing_file.IsInvalid()) {
					g_backing_file.Seek(foff);
					uint32_t wr = 0;
					g_backing_file.Write(buf.data(), static_cast<uint32_t>(size), &wr);
					if (wr < size) {
						LOGF("\tNVMe write incomplete: %u/%" PRIu64 " bytes\n", wr, size);
					}
				}
			}
			CompleteCommand(sq, cq, cmd->command_id, 0);
			break;
		}
		case NvmeNvmOpcode::WriteZeroes:
		case NvmeNvmOpcode::DatasetManagement:
		case NvmeNvmOpcode::WriteUncorrectable:
			CompleteCommand(sq, cq, cmd->command_id, 0);
			break;
		default:
			CompleteCommand(sq, cq, cmd->command_id, 0x0002);
			break;
	}
}

// =============================================================================
// Public API
// =============================================================================

KYTY_SUBSYSTEM_INIT(NvmeStorage) {
	// Initialize with default empty state
	g_ctrl = {};
	g_ctrl.initialized = true;
	LOGF("NVMe storage subsystem initialized\n");
}

KYTY_SUBSYSTEM_UNEXPECTED_SHUTDOWN(NvmeStorage) {
	Common::LockGuard bl(g_backing_mutex);
	if (!g_backing_file.IsInvalid()) {
		g_backing_file.Flush();
		g_backing_file.Close();
	}
}

KYTY_SUBSYSTEM_DESTROY(NvmeStorage) {
	Common::LockGuard bl(g_backing_mutex);
	if (!g_backing_file.IsInvalid()) {
		g_backing_file.Flush();
		g_backing_file.Close();
	}
	g_initialized = false;
}

void Initialize(const std::filesystem::path& backing_path, uint64_t size_bytes) {
	Common::LockGuard bl(g_backing_mutex);

	g_ctrl.namespace_size = size_bytes;
	g_ctrl.namespace_lbas = size_bytes / NVME_SECTOR_SIZE;

	bool exists = Common::File::IsFileExisting(backing_path);
	Common::File::CreateDirectories(backing_path.parent_path());

	if (!exists) {
		if (!g_backing_file.Create(backing_path)) {
			LOGF_COLOR(Log::Color::Red, "NVMe: failed to create backing file: %s\n",
			           Common::PathToString(backing_path).c_str());
			return;
		}
		// Extend to desired size using a custom method or loop
		// since SetSize is missing from Common::File
		uint8_t zero = 0;
		g_backing_file.Seek(size_bytes - 1);
		g_backing_file.Write(&zero, 1, nullptr);
		LOGF("NVMe: created backing file %s (%" PRIu64 " MiB)\n",
		     Common::PathToString(backing_path).c_str(), size_bytes / (1024 * 1024));
	} else {
		if (!g_backing_file.Open(backing_path, Common::File::Mode::ReadWrite)) {
			LOGF_COLOR(Log::Color::Red, "NVMe: failed to open backing file: %s\n",
			           Common::PathToString(backing_path).c_str());
			return;
		}
		LOGF("NVMe: opened backing file %s (%" PRIu64 " MiB)\n",
		     Common::PathToString(backing_path).c_str(), size_bytes / (1024 * 1024));
	}

	g_ctrl.backing_file = backing_path;
	g_initialized = true;
}

void Shutdown() {
	Common::LockGuard bl(g_backing_mutex);
	if (!g_backing_file.IsInvalid()) {
		g_backing_file.Flush();
		g_backing_file.Close();
	}
	g_initialized = false;
}

void SetInterruptCallback(NvmeInterruptCallback callback) {
	g_interrupt_callback = std::move(callback);
}

uint64_t GetMMIOBaseAddress() { return 0; }
uint64_t GetMMIOSize() { return NVME_MMIO_BAR_SIZE; }
uint32_t GetMaxNamespaces() { return 1; }
uint64_t GetNamespaceSize(uint32_t nsid) {
	(void)nsid;
	return g_ctrl.namespace_size;
}

bool IsControllerReady() { return g_ctrl.controller_enabled; }

bool StorageRead(uint64_t lba, uint32_t num_sectors, void* buffer) {
	if (!buffer) return false;
	Common::LockGuard bl(g_backing_mutex);
	if (g_backing_file.IsInvalid()) return false;
	g_backing_file.Seek(lba * NVME_SECTOR_SIZE);
	uint32_t rd = 0;
	g_backing_file.Read(buffer, num_sectors * NVME_SECTOR_SIZE, &rd);
	return rd == num_sectors * NVME_SECTOR_SIZE;
}

bool StorageWrite(uint64_t lba, uint32_t num_sectors, const void* buffer) {
	if (!buffer) return false;
	Common::LockGuard bl(g_backing_mutex);
	if (g_backing_file.IsInvalid()) return false;
	g_backing_file.Seek(lba * NVME_SECTOR_SIZE);
	uint32_t wr = 0;
	g_backing_file.Write(buffer, num_sectors * NVME_SECTOR_SIZE, &wr);
	return wr == num_sectors * NVME_SECTOR_SIZE;
}

bool StorageFlush() {
	Common::LockGuard bl(g_backing_mutex);
	if (g_backing_file.IsInvalid()) return false;
	g_backing_file.Flush();
	return true;
}

} // namespace Libs::LibKernel::Nvme
