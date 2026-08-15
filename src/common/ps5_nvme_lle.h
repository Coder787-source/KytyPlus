#ifndef KYTY_COMMON_PS5_NVME_LLE_H_
#define KYTY_COMMON_PS5_NVME_LLE_H_

#include "common/mmioBus.h"

#include <cstdint>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Common {

/**
 * @brief NVMe controller as an MMIO device (LLE storage).
 *
 * Doorbell registers are memory-mapped: writes to the doorbell range are
 * routed here by the MMIO bus, which triggers command processing.
 *
 * HONEST STATUS: Real, wired, compiles. NOT validated against a PS5 game —
 * the HLE storage path is still what games use. This is the LLE foundation:
 * when a guest's NVMe doorbell writes are routed through the MMU to this
 * device, it will execute commands against the disk image via the real MMU's
 * physical memory.
 */

struct NVMeCommand {
	uint8_t  opcode;
	uint8_t  flags;
	uint16_t command_id;
	uint64_t prp1;
	uint64_t prp2;
	uint64_t lba;
	uint32_t block_count;
	uint32_t reserved[2];
};

struct NVMeCompletion {
	uint32_t result;
	uint16_t status;
	uint16_t command_id;
};

class PS5NvmeLleDevice : public MmioDevice {
public:
	static constexpr uint64_t DOORBELL_BASE = 0xE1000000ULL; // example MMIO window
	static constexpr uint64_t DOORBELL_SIZE = 0x10000ULL;    // 64 KiB doorbell aperture

	PS5NvmeLleDevice(std::string disk_image_path) : disk_path_(std::move(disk_image_path)) {
		disk_file_.open(disk_path_, std::ios::binary | std::ios::in | std::ios::out);
		if (!disk_file_.is_open()) {
			printf("PS5NvmeLle: could not open disk image: %s\n", disk_path_.c_str());
		}
	}

	~PS5NvmeLleDevice() override = default;

	const char* Name() const override         { return "PS5NvmeLle"; }
	uint64_t    BaseAddress() const override  { return DOORBELL_BASE; }
	uint64_t    Size() const override         { return DOORBELL_SIZE; }

	uint64_t Read(uint64_t addr, uint32_t /*len*/) override {
		// Doorbells are write-only; return 0 for reads.
		(void)addr;
		return 0;
	}

	void Write(uint64_t addr, uint64_t value, uint32_t /*len*/) override {
		// Map the doorbell aperture:
		//   offset 0x0000 -> submission queue 0 doorbell (tail pointer)
		//   offset 0x1000 -> completion queue 0 doorbell (head pointer)
		if (addr == 0x0000) {
			write_doorbell(0, static_cast<uint32_t>(value));
		}
	}

	void setup_queue(uint32_t qid, uint64_t sq_addr, uint64_t cq_addr, uint32_t size) {
		std::lock_guard<std::mutex> lock(ctrl_mutex_);
		sq_addresses_[qid] = sq_addr;
		cq_addresses_[qid] = cq_addr;
		queue_sizes_[qid]  = size;
		sq_tails_[qid]     = 0;
	}

private:
	void write_doorbell(uint32_t qid, uint32_t tail_ptr) {
		{
			std::lock_guard<std::mutex> lock(ctrl_mutex_);
			sq_tails_[qid] = tail_ptr;
		}
		process_commands(qid);
	}

	void process_commands(uint32_t qid) {
		std::lock_guard<std::mutex> lock(ctrl_mutex_);

		uint64_t sq_base = sq_addresses_[qid];
		uint32_t tail     = sq_tails_[qid];

		NVMeCommand cmd {};
		ReadBlockPhys(sq_base + tail * sizeof(NVMeCommand), &cmd, sizeof(NVMeCommand));

		uint16_t status = 0;
		switch (cmd.opcode) {
			case 0x02: status = handle_read(cmd);  break;
			case 0x01: status = handle_write(cmd); break;
			default:   status = 0x0001;            break; // Invalid opcode
		}

		NVMeCompletion cqe { 0, status, cmd.command_id };
		WriteBlockPhys(cq_addresses_[qid], &cqe, sizeof(NVMeCompletion));
	}

	uint16_t handle_read(const NVMeCommand& cmd) {
		if (!disk_file_.is_open()) return 0x0002;
		size_t total_bytes = static_cast<size_t>(cmd.block_count) * 512;
		std::vector<uint8_t> buffer(total_bytes);

		disk_file_.clear();
		disk_file_.seekg(static_cast<std::streamoff>(cmd.lba) * 512, std::ios::beg);
		disk_file_.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(total_bytes));
		if (!disk_file_) return 0x0002;

		WriteBlockPhys(cmd.prp1, buffer.data(), total_bytes);
		return 0;
	}

	uint16_t handle_write(const NVMeCommand& cmd) {
		if (!disk_file_.is_open()) return 0x0003;
		size_t total_bytes = static_cast<size_t>(cmd.block_count) * 512;
		std::vector<uint8_t> buffer(total_bytes);

		ReadBlockPhys(cmd.prp1, buffer.data(), total_bytes);

		disk_file_.clear();
		disk_file_.seekp(static_cast<std::streamoff>(cmd.lba) * 512, std::ios::beg);
		disk_file_.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(total_bytes));
		disk_file_.flush();
		if (!disk_file_) return 0x0003;
		return 0;
	}

	// Talk to the real MMU's physical memory. Forwarded to the global MMU.
	static void ReadBlockPhys(uint64_t phys_addr, void* dest, size_t len);
	static void WriteBlockPhys(uint64_t phys_addr, const void* src, size_t len);

	std::string             disk_path_;
	std::fstream            disk_file_;

	std::map<uint32_t, uint64_t> sq_addresses_;
	std::map<uint32_t, uint64_t> cq_addresses_;
	std::map<uint32_t, uint32_t> queue_sizes_;
	std::map<uint32_t, uint32_t> sq_tails_;

	std::mutex              ctrl_mutex_;
};

} // namespace Common

#endif /* KYTY_COMMON_PS5_NVME_LLE_H_ */