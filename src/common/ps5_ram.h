#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <iostream>

namespace KytyPS5 {

    /**
     * @brief Accurate representation of PS5 System RAM.
     * Focuses on structural accuracy for DMA operations.
     */
    class PS5Ram {
    public:
        explicit PS5Ram(uint64_t size) : size_(size) {
            // Using unique_ptr for large allocation to avoid stack overflow and ensure alignment
            memory_ = std::make_unique<uint8_t[]>(size);
            std::memset(memory_.get(), 0, size);
        }

        // CPU Access (Accurate 8-bit read/write)
        void write8(uint64_t addr, uint8_t val) {
            validate_addr(addr, 1);
            std::lock_guard<std::mutex> lock(mem_mutex_);
            memory_[addr] = val;
        }

        uint8_t read8(uint64_t addr) const {
            validate_addr(addr, 1);
            std::lock_guard<std::mutex> lock(mem_mutex_);
            return memory_[addr];
        }

        // DMA Operations (Critical for NVMe behavior)
        void dma_write(uint64_t addr, const uint8_t* src, size_t length) {
            validate_addr(addr, length);
            std::lock_guard<std::mutex> lock(mem_mutex_);
            std::memcpy(memory_.get() + addr, src, length);
        }

        void dma_read(uint64_t addr, uint8_t* dest, size_t length) const {
            validate_addr(addr, length);
            std::lock_guard<std::mutex> lock(mem_mutex_);
            std::memcpy(dest, memory_.get() + addr, length);
        }

        uint64_t get_size() const { return size_; }

    private:
        void validate_addr(uint64_t addr, size_t len) const {
            if (addr + len > size_) {
                std::cerr << "PS5 RAM Access Violation: Out of physical bounds\n";
            }
        }

        uint64_t size_;
        std::unique_ptr<uint8_t[]> memory_;
        mutable std::mutex mem_mutex_; // Ensure thread safety for DMA vs CPU access
    };

}
