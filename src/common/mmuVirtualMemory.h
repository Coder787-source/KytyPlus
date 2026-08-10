#pragma once

#include "common.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <functional>

namespace Kyty::Common {

// MMU/Virtual Memory System
// Provides 16GB virtual address space with page table management

constexpr uint64_t MMU_PAGE_SIZE = 4096;
constexpr uint64_t MMU_LARGE_PAGE_SIZE = 2 * 1024 * 1024; // 2MB
constexpr uint64_t MMU_VIRTUAL_SIZE = 16ULL * 1024 * 1024 * 1024; // 16GB
constexpr uint64_t MMU_PHYSICAL_SIZE = 16ULL * 1024 * 1024 * 1024; // 16GB (matches PS5)

enum class MemoryProtection {
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4,
    ReadWrite = 3,
    ReadExecute = 5,
    ReadWriteExecute = 7
};

enum class MemoryType {
    Private,
    Mapped,
    Shared,
    Reserve,
    Commit,
    Device
};

struct MemoryPage {
    uint64_t virtualAddress = 0;
    uint64_t physicalAddress = 0;
    MemoryProtection protection = MemoryProtection::None;
    MemoryType type = MemoryType::Private;
    bool isPresent = false;
    bool isDirty = false;
    bool isAccessed = false;
    bool isLargePage = false;
    uint8_t* data = nullptr;
};

struct MemoryRegion {
    uint64_t baseAddress = 0;
    uint64_t size = 0;
    MemoryProtection protection = MemoryProtection::None;
    MemoryType type = MemoryType::Private;
    std::string name;
    bool isMapped = false;
    int32_t fd = -1; // File descriptor for mapped regions
    int64_t offset = 0; // Offset in mapped file
};

struct MMUStats {
    uint64_t totalVirtualPages = 0;
    uint64_t usedVirtualPages = 0;
    uint64_t totalPhysicalPages = 0;
    uint64_t usedPhysicalPages = 0;
    uint64_t pageFaults = 0;
    uint64_t pageIns = 0;
    uint64_t pageOuts = 0;
    uint64_t largePages = 0;
    uint64_t compressedPages = 0;
};

class MMU {
public:
    MMU();
    ~MMU();

    // Initialization
    bool Initialize(uint64_t virtualSize = MMU_VIRTUAL_SIZE,
                    uint64_t physicalSize = MMU_PHYSICAL_SIZE);
    void Shutdown();

    // Memory allocation
    uint64_t Allocate(uint64_t size, MemoryProtection protection = MemoryProtection::ReadWrite,
                      MemoryType type = MemoryType::Private, uint64_t alignment = MMU_PAGE_SIZE);
    uint64_t AllocateAt(uint64_t address, uint64_t size, MemoryProtection protection = MemoryProtection::ReadWrite,
                        MemoryType type = MemoryType::Private);
    void Free(uint64_t address);
    void FreeRange(uint64_t address, uint64_t size);

    // Memory mapping
    uint64_t MapFile(const std::string& filePath, uint64_t offset, uint64_t size,
                     MemoryProtection protection = MemoryProtection::Read);
    void Unmap(uint64_t address);

    // Memory protection
    bool Protect(uint64_t address, uint64_t size, MemoryProtection protection);
    MemoryProtection GetProtection(uint64_t address) const;

    // Address translation
    uint64_t VirtualToPhysical(uint64_t virtualAddress) const;
    uint8_t* GetPointer(uint64_t virtualAddress) const;
    
    // Template helpers for typed access
    template<typename T>
    T Read(uint64_t address) const {
        const uint8_t* ptr = GetPointer(address);
        return ptr ? *reinterpret_cast<const T*>(ptr) : T{};
    }

    template<typename T>
    void Write(uint64_t address, T value) {
        uint8_t* ptr = GetPointer(address);
        if (ptr) {
            *reinterpret_cast<T*>(ptr) = value;
        }
    }

    // Block operations
    void ReadBlock(uint64_t address, void* dest, uint64_t size) const;
    void WriteBlock(uint64_t address, const void* src, uint64_t size);
    void Zero(uint64_t address, uint64_t size);
    void Copy(uint64_t dest, uint64_t src, uint64_t size);

    // Page management
    bool CommitPage(uint64_t virtualAddress);
    void DecommitPage(uint64_t virtualAddress);
    bool IsPagePresent(uint64_t virtualAddress) const;

    // Large page support
    uint64_t AllocateLargePage(uint64_t size, MemoryProtection protection = MemoryProtection::ReadWrite);
    void FreeLargePage(uint64_t address);

    // Memory compression
    void EnableCompression(bool enable);
    bool IsCompressionEnabled() const { return m_compressionEnabled; }
    uint64_t GetCompressedPageCount() const;

    // Demand paging
    void SetPageFaultHandler(std::function<void(uint64_t, bool)> handler);
    void TriggerPageFault(uint64_t virtualAddress, bool isWrite);

    // Statistics
    MMUStats GetStats() const;
    void ResetStats();
    uint64_t GetUsedMemory() const;
    uint64_t GetFreeMemory() const;
    double GetUtilization() const;

    // Debugging
    void DumpRegions() const;
    bool IsValidAddress(uint64_t address) const;
    MemoryRegion* GetRegion(uint64_t address);

private:
    uint64_t AlignUp(uint64_t value, uint64_t alignment) const;
    uint64_t AlignDown(uint64_t value, uint64_t alignment) const;
    uint32_t GetPageIndex(uint64_t address) const;
    uint64_t GetPageAddress(uint32_t pageIndex) const;
    MemoryPage* GetPage(uint64_t virtualAddress);
    const MemoryPage* GetPage(uint64_t virtualAddress) const;
    void UpdateStats();

    bool m_initialized = false;
    bool m_compressionEnabled = false;

    uint64_t m_virtualSize = 0;
    uint64_t m_physicalSize = 0;
    uint32_t m_totalPages = 0;

    std::vector<MemoryPage> m_pageTable;
    std::vector<MemoryRegion> m_regions;
    std::vector<uint8_t> m_physicalMemory;
    
    std::unordered_map<uint64_t, uint64_t> m_virtualToPhysicalMap;
    std::unordered_map<uint64_t, std::vector<uint8_t>> m_compressedPages;

    std::function<void(uint64_t, bool)> m_pageFaultHandler;

    mutable std::mutex m_mutex;
    MMUStats m_stats;

    uint64_t m_nextAddress = 0x400000; // Start after system reserved
};

// Global MMU instance
MMU& GetMMU();

} // namespace Kyty::Common
