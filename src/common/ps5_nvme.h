#pragma once

#include "ps5_ram.h"
#include <string>
#include <fstream>
#include <map>
#include <mutex>

namespace KytyPS5 {

    // NVMe Command Structure (Hardware Accurate)
    struct NVMeCommand {
        uint8_t opcode;
        uint8_t flags;
        uint16_t command_id;
        uint64_t prp1;       // Physical Region Page 1 (RAM pointer)
        uint64_t prp2;       // Physical Region Page 2
        uint64_t lba;        // Logical Block Address
        uint32_t block_count; 
        uint32_t reserved[2];
    };

    // NVMe Completion Queue Entry
    struct NVMeCompletion {
        uint32_t result;
        uint16_t status;
        uint16_t command_id;
    };

    /**
     * @brief Accurate NVMe Controller emulation.
     * Implements Queue/Doorbell mechanism and DMA data movement.
     */
    class PS5NvmeController {
    public:
        PS5NvmeController(PS5Ram& ram, const std::string& disk_image_path) 
            : ram_(ram), disk_path_(disk_image_path) {
            
            disk_file_.open(disk_path_, std::ios::binary | std::ios::in | std::ios::out);
            if (!disk_file_.is_open()) {
                throw std::runtime_error("NVMe Controller failed to attach to disk image: " + disk_path_);
            }
        }

        ~PS5NvmeController() {
            if (disk_file_.is_open()) disk_file_.close();
        }

        // Setup Queue: Mapping RAM addresses to the controller's internal state
        void setup_queue(uint32_t qid, uint64_t sq_addr, uint64_t cq_addr, uint32_t size) {
            std::lock_guard<std::mutex> lock(ctrl_mutex_);
            sq_addresses_[qid] = sq_addr;
            cq_addresses_[qid] = cq_addr;
            queue_sizes_[qid] = size;
            sq_tails_[qid] = 0;
        }

        // Trigger: The CPU writes to the Doorbell register (MMIO)
        void write_doorbell(uint32_t qid, uint32_t tail_ptr) {
            {
                std::lock_guard<std::mutex> lock(ctrl_mutex_);
                sq_tails_[qid] = tail_ptr;
            }
            process_commands();
        }

    private:
        void process_commands() {
            std::lock_guard<std::mutex> lock(ctrl_mutex_);
            for (auto const& [qid, tail] : sq_tails_) {
                uint64_t sq_base = sq_addresses_[qid];
                
                // 1. Fetch Command from RAM via DMA
                NVMeCommand cmd;
                ram_.dma_read(sq_base + (sq_tails_[qid] * sizeof(NVMeCommand)), 
                             reinterpret_cast<uint8_t*>(&cmd), sizeof(NVMeCommand));

                // 2. Execute Command based on Opcode
                uint16_t status = 0;
                if (cmd.opcode == 0x02) { // Read
                    status = handle_read(cmd);
                } else if (cmd.opcode == 0x01) { // Write
                    status = handle_write(cmd);
                } else {
                    status = 0x0001; // Invalid Opcode
                }

                // 3. Post Completion to CQ in RAM via DMA
                NVMeCompletion cqe = {0, status, cmd.command_id};
                ram_.dma_write(cq_addresses_[qid], reinterpret_cast<uint8_t*>(&cqe), sizeof(NVMeCompletion));
            }
        }

        uint16_t handle_read(const NVMeCommand& cmd) {
            size_t total_bytes = cmd.block_count * 512;
            std::vector<uint8_t> buffer(total_bytes);

            disk_file_.seekg(cmd.lba * 512, std::ios::beg);
            disk_file_.read(reinterpret_cast<char*>(buffer.data()), total_bytes);

            if (!disk_file_) return 0x0002; // Read Error

            // DMA transfer from controller to system RAM
            ram_.dma_write(cmd.prp1, buffer.data(), total_bytes);
            return 0;
        }

        uint16_t handle_write(const NVMeCommand& cmd) {
            size_t total_bytes = cmd.block_count * 512;
            std::vector<uint8_t> buffer(total_bytes);

            // DMA transfer from system RAM to controller
            ram_.dma_read(cmd.prp1, buffer.data(), total_bytes);

            disk_file_.seekp(cmd.lba * 512, std::ios::beg);
            disk_file_.write(reinterpret_cast<char*>(buffer.data()), total_bytes);
            disk_file_.flush();

            if (!disk_file_) return 0x0003; // Write Error
            return 0;
        }

        PS5Ram& ram_;
        std::string disk_path_;
        std::fstream disk_file_;
        
        std::map<uint32_t, uint64_t> sq_addresses_;
        std::map<uint32_t, uint64_t> cq_addresses_;
        std::map<uint32_t, uint32_t> queue_sizes_;
        std::map<uint32_t, uint32_t> sq_tails_;
        
        std::mutex ctrl_mutex_;
    };

}
