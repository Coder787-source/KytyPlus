#include "ps5_ram.h"
#include "ps5_nvme.h"
#include <iostream>
#include <cassert>
#include <fstream>

int main() {
    try {
        std::cout << "[Test] Initializing PS5 Hardware Emulation..." << std::endl;

        // 1. Setup 16GB RAM
        KytyPS5::PS5Ram ram(16ULL * 1024 * 1024 * 1024);
        
        // Create a mock disk image for testing
        const std::string disk_path = "test_disk.bin";
        std::ofstream create_disk(disk_path, std::ios::binary);
        std::string secret_data = "PS5_SAMSUNG_NVME_BLOCK_DATA_ACCURACY_TEST";
        create_disk.write(secret_data.c_str(), secret_data.size());
        // Ensure file is at least 512 bytes for block alignment
        std::vector<char> padding(512 - secret_data.size(), 0);
        create_disk.write(padding.data(), padding.size());
        create_disk.close();

        // 2. Setup NVMe Controller
        KytyPS5::PS5NvmeController nvme(ram, disk_path);

        // 3. Initialize Queues in RAM
        uint64_t sq_addr = 0x10000;
        uint64_t cq_addr = 0x20000;
        nvme.setup_queue(1, sq_addr, cq_addr, 64);

        // 4. Simulate OS: Prepare a READ command in RAM
        KytyPS5::NVMeCommand read_cmd;
        read_cmd.opcode = 0x02;      // Read Opcode
        read_cmd.command_id = 0xABC;
        read_cmd.lba = 0;           // Block 0
        read_cmd.block_count = 1;    // 1 Sector (512 bytes)
        read_cmd.prp1 = 0x50000;     // Destination RAM address

        // DMA the command into the Submission Queue (SQ)
        ram.dma_write(sq_addr, reinterpret_cast<uint8_t*>(&read_cmd), sizeof(KytyPS5::NVMeCommand));

        // 5. Simulate OS: Write to Doorbell Register (Trigger NVMe)
        std::cout << "[Test] Writing to Doorbell register..." << std::endl;
        nvme.write_doorbell(1, 0);

        // 6. Verification
        uint8_t verification_buffer[64];
        ram.dma_read(0x50000, verification_buffer, 64);

        std::cout << "[Test] RAM Data at 0x50000: " << std::string((char*)verification_buffer) << std::endl;

        if (std::string((char*)verification_buffer).find("PS5_SAMSUNG") != std::string::npos) {
            std::cout << "[RESULT] TEST PASSED: Hardware behavior is accurate." << std::endl;
        } else {
            std::cout << "[RESULT] TEST FAILED: Data mismatch." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL ERROR] " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
