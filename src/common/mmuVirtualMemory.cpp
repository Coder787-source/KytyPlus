#include "common/mmuVirtualMemory.h"
#include "common/log.h"
#include "common/assert.h"
#include <algorithm>
#include <cstring>

namespace Kyty::Common {

// Global MMU instance
static MMU g_mmu;

MMU& GetMMU() {
    return g_mmu;
}

MMU::MMU() {
    m_stats = MMUStats{};
}

MMU::~MMU() {
    Shutdown();
}

bool MMU::Initialize(uint64_t virtualSize, uint64_t physicalSize) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        LOG_WARNING("MMU", "Already initialized");
        return true;
    }

    m_virtualSize = AlignUp(virtualSize, MMU_PAGE_SIZE);
    m_physicalSize = AlignUp(physicalSize, MMU_PAGE_SIZE);

    m_totalPages = static_cast<uint32_t>(m_virtualSize / MMU_PAGE_SIZE);

    LOG_INFO("MMU", "Initializing: Virtual=%llu GB, Physical=%llu GB, Pages=%u",
             m_virtualSize / (1024 * 1024 * 1024),
             m_physicalSize / (1024 * 1024 * 1024),
             m_totalPages);

    // Initialize page table
    m_pageTable.resize(m_totalPages);
    for (uint32_t i = 0; i < m_totalPages; i++) {
        m_pageTable[i].virtualAddress = GetPageAddress(i);
        m_pageTable[i].physicalAddress = 0;
        m_pageTable[i].protection = MemoryProtection::None;
        m_pageTable[i].type = MemoryType::Private;
        m_pageTable[i].isPresent = false;
        m_pageTable[i].isDirty = false;
        m_pageTable[i].isAccessed = false;
        m_pageTable[i].isLargePage = false;
        m_pageTable[i].data = nullptr;
    }

    // Allocate physical memory
    // In production, this would use large pages and NUMA-aware allocation
    m_physicalMemory.resize(m_physicalSize);
    LOG_INFO("MMU", "Physical memory allocated: %zu bytes", m_physicalMemory.size());

    // Initialize address space
    m_nextAddress = 0x400000; // Start after system reserved (4MB)

    m_initialized = true;
    ResetStats();

    LOG_INFO("MMU", "Initialization complete");
    return true;
}

void MMU::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return;
    }

    // Free all allocated pages
    for (auto& page : m_pageTable) {
        if (page.data && page.type != MemoryType::Mapped) {
            delete[] page.data;
            page.data = nullptr;
        }
    }

    m_pageTable.clear();
    m_physicalMemory.clear();
    m_regions.clear();
    m_virtualToPhysicalMap.clear();
    m_compressedPages.clear();

    m_initialized = false;

    LOG_INFO("MMU", "Shutdown complete");
}

uint64_t MMU::Allocate(uint64_t size, MemoryProtection protection, MemoryType type, uint64_t alignment) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || size == 0) {
        return 0;
    }

    size = AlignUp(size, alignment);
    uint64_t baseAddress = AlignUp(m_nextAddress, alignment);

    // Find contiguous free region
    uint64_t endAddress = baseAddress + size;
    if (endAddress > m_virtualSize) {
        LOG_ERROR("MMU", "Out of virtual memory");
        return 0;
    }

    // Allocate pages
    for (uint64_t addr = baseAddress; addr < endAddress; addr += MMU_PAGE_SIZE) {
        MemoryPage* page = GetPage(addr);
        if (!page) {
            LOG_ERROR("MMU", "Invalid page address: 0x%llx", addr);
            // Free already allocated pages
            FreeRange(baseAddress, addr - baseAddress);
            return 0;
        }

        if (page->isPresent) {
            LOG_ERROR("MMU", "Page already allocated at 0x%llx", addr);
            FreeRange(baseAddress, addr - baseAddress);
            return 0;
        }

        // Commit the page
        if (!CommitPage(addr)) {
            LOG_ERROR("MMU", "Failed to commit page at 0x%llx", addr);
            FreeRange(baseAddress, addr - baseAddress);
            return 0;
        }

        page->protection = protection;
        page->type = type;
        page->isAccessed = false;
        page->isDirty = false;
    }

    // Create region
    MemoryRegion region;
    region.baseAddress = baseAddress;
    region.size = size;
    region.protection = protection;
    region.type = type;
    region.isMapped = false;
    m_regions.push_back(region);

    m_nextAddress = endAddress;
    UpdateStats();

    LOG_DEBUG("MMU", "Allocated %llu bytes at 0x%llx", size, baseAddress);
    return baseAddress;
}

uint64_t MMU::AllocateAt(uint64_t address, uint64_t size, MemoryProtection protection, MemoryType type) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || address == 0 || size == 0) {
        return 0;
    }

    address = AlignUp(address, MMU_PAGE_SIZE);
    size = AlignUp(size, MMU_PAGE_SIZE);

    // Check if region is free
    for (uint64_t addr = address; addr < address + size; addr += MMU_PAGE_SIZE) {
        MemoryPage* page = GetPage(addr);
        if (!page || page->isPresent) {
            LOG_ERROR("MMU", "Region not free at 0x%llx", addr);
            return 0;
        }
    }

    // Allocate pages at specified address
    for (uint64_t addr = address; addr < address + size; addr += MMU_PAGE_SIZE) {
        if (!CommitPage(addr)) {
            LOG_ERROR("MMU", "Failed to commit page at 0x%llx", addr);
            return 0;
        }

        MemoryPage* page = GetPage(addr);
        if (page) {
            page->protection = protection;
            page->type = type;
        }
    }

    // Create region
    MemoryRegion region;
    region.baseAddress = address;
    region.size = size;
    region.protection = protection;
    region.type = type;
    region.isMapped = false;
    m_regions.push_back(region);

    UpdateStats();

    LOG_DEBUG("MMU", "Allocated %llu bytes at 0x%llx (requested)", size, address);
    return address;
}

void MMU::Free(uint64_t address) {
    if (address == 0) {
        return;
    }

    MemoryRegion* region = GetRegion(address);
    if (region) {
        FreeRange(region->baseAddress, region->size);
    }
}

void MMU::FreeRange(uint64_t address, uint64_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || address == 0 || size == 0) {
        return;
    }

    address = AlignUp(address, MMU_PAGE_SIZE);
    size = AlignUp(size, MMU_PAGE_SIZE);

    // Free pages
    for (uint64_t addr = address; addr < address + size; addr += MMU_PAGE_SIZE) {
        MemoryPage* page = GetPage(addr);
        if (page && page->isPresent) {
            DecommitPage(addr);
        }
    }

    // Remove region
    auto it = std::find_if(m_regions.begin(), m_regions.end(),
                           [address](const MemoryRegion& region) {
                               return region.baseAddress == address;
                           });

    if (it != m_regions.end()) {
        m_regions.erase(it);
    }

    UpdateStats();

    LOG_DEBUG("MMU", "Freed %llu bytes at 0x%llx", size, address);
}

uint64_t MMU::MapFile(const std::string& filePath, uint64_t offset, uint64_t size,
                      MemoryProtection protection) {
    // File mapping stub - in production would use CreateFileMapping/MapViewOfFile
    LOG_INFO("MMU", "MapFile: %s offset=%llu size=%llu (stub)", filePath.c_str(), offset, size);

    // Allocate memory for mapped region
    uint64_t address = Allocate(size, protection, MemoryType::Mapped);
    if (address == 0) {
        return 0;
    }

    // Update region with mapping info
    MemoryRegion* region = GetRegion(address);
    if (region) {
        region->isMapped = true;
        region->fd = -1; // Would be real FD in production
        region->offset = offset;
    }

    // In production: actually map the file
    // For now, zero-initialize
    Zero(address, size);

    return address;
}

void MMU::Unmap(uint64_t address) {
    if (address == 0) {
        return;
    }

    MemoryRegion* region = GetRegion(address);
    if (region && region->isMapped) {
        FreeRange(region->baseAddress, region->size);
    }
}

bool MMU::Protect(uint64_t address, uint64_t size, MemoryProtection protection) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || address == 0 || size == 0) {
        return false;
    }

    address = AlignUp(address, MMU_PAGE_SIZE);
    size = AlignUp(size, MMU_PAGE_SIZE);

    for (uint64_t addr = address; addr < address + size; addr += MMU_PAGE_SIZE) {
        MemoryPage* page = GetPage(addr);
        if (!page || !page->isPresent) {
            return false;
        }
        page->protection = protection;
    }

    LOG_DEBUG("MMU", "Changed protection at 0x%llx to %d", address, static_cast<int>(protection));
    return true;
}

MemoryProtection MMU::GetProtection(uint64_t address) const {
    const MemoryPage* page = GetPage(address);
    return page ? page->protection : MemoryProtection::None;
}

uint64_t MMU::VirtualToPhysical(uint64_t virtualAddress) const {
    const MemoryPage* page = GetPage(virtualAddress);
    if (!page || !page->isPresent) {
        return 0;
    }

    uint64_t offset = virtualAddress % MMU_PAGE_SIZE;
    return page->physicalAddress + offset;
}

uint8_t* MMU::GetPointer(uint64_t virtualAddress) const {
    const MemoryPage* page = GetPage(virtualAddress);
    if (!page || !page->isPresent || !page->data) {
        return nullptr;
    }

    uint64_t offset = virtualAddress % MMU_PAGE_SIZE;
    return page->data + offset;
}

void MMU::ReadBlock(uint64_t address, void* dest, uint64_t size) const {
    if (!dest || size == 0) {
        return;
    }

    uint8_t* destPtr = static_cast<uint8_t*>(dest);
    uint64_t remaining = size;
    uint64_t offset = 0;

    while (remaining > 0) {
        uint64_t pageSize = MMU_PAGE_SIZE - (address % MMU_PAGE_SIZE);
        uint64_t chunkSize = std::min(remaining, pageSize);

        const uint8_t* src = GetPointer(address);
        if (!src) {
            LOG_ERROR("MMU", "Invalid read at 0x%llx", address);
            std::memset(destPtr + offset, 0, remaining);
            return;
        }

        std::memcpy(destPtr + offset, src, chunkSize);

        address += chunkSize;
        offset += chunkSize;
        remaining -= chunkSize;
    }
}

void MMU::WriteBlock(uint64_t address, const void* src, uint64_t size) {
    if (!src || size == 0) {
        return;
    }

    const uint8_t* srcPtr = static_cast<const uint8_t*>(src);
    uint64_t remaining = size;
    uint64_t offset = 0;

    while (remaining > 0) {
        uint64_t pageSize = MMU_PAGE_SIZE - (address % MMU_PAGE_SIZE);
        uint64_t chunkSize = std::min(remaining, pageSize);

        uint8_t* dest = GetPointer(address);
        if (!dest) {
            LOG_ERROR("MMU", "Invalid write at 0x%llx", address);
            return;
        }

        std::memcpy(dest, srcPtr + offset, chunkSize);

        // Mark page as dirty
        MemoryPage* page = GetPage(address);
        if (page) {
            page->isDirty = true;
        }

        address += chunkSize;
        offset += chunkSize;
        remaining -= chunkSize;
    }
}

void MMU::Zero(uint64_t address, uint64_t size) {
    std::vector<uint8_t> buffer(size);
    std::memset(buffer.data(), 0, size);
    WriteBlock(address, buffer.data(), size);
}

void MMU::Copy(uint64_t dest, uint64_t src, uint64_t size) {
    std::vector<uint8_t> buffer(size);
    ReadBlock(src, buffer.data(), size);
    WriteBlock(dest, buffer.data(), size);
}

bool MMU::CommitPage(uint64_t virtualAddress) {
    MemoryPage* page = GetPage(virtualAddress);
    if (!page) {
        return false;
    }

    if (page->isPresent) {
        return true; // Already committed
    }

    // Allocate physical memory for the page
    page->data = new uint8_t[MMU_PAGE_SIZE];
    std::memset(page->data, 0, MMU_PAGE_SIZE);

    // Assign physical address
    page->physicalAddress = reinterpret_cast<uint64_t>(page->data);
    page->isPresent = true;
    page->isAccessed = false;
    page->isDirty = false;

    m_stats.pageIns++;

    return true;
}

void MMU::DecommitPage(uint64_t virtualAddress) {
    MemoryPage* page = GetPage(virtualAddress);
    if (!page || !page->isPresent) {
        return;
    }

    // Free physical memory
    if (page->data && page->type != MemoryType::Mapped) {
        delete[] page->data;
    }

    page->data = nullptr;
    page->physicalAddress = 0;
    page->isPresent = false;
    page->isDirty = false;

    m_stats.pageOuts++;
}

bool MMU::IsPagePresent(uint64_t virtualAddress) const {
    const MemoryPage* page = GetPage(virtualAddress);
    return page && page->isPresent;
}

uint64_t MMU::AllocateLargePage(uint64_t size, MemoryProtection protection) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || size == 0) {
        return 0;
    }

    size = AlignUp(size, MMU_LARGE_PAGE_SIZE);
    uint64_t baseAddress = AlignUp(m_nextAddress, MMU_LARGE_PAGE_SIZE);

    // Allocate contiguous large pages
    for (uint64_t addr = baseAddress; addr < baseAddress + size; addr += MMU_LARGE_PAGE_SIZE) {
        // Allocate 512 regular pages as one large page
        for (uint64_t i = 0; i < 512; i++) {
            uint64_t pageAddr = addr + i * MMU_PAGE_SIZE;
            MemoryPage* page = GetPage(pageAddr);

            if (!page || page->isPresent) {
                LOG_ERROR("MMU", "Cannot allocate large page at 0x%llx", addr);
                return 0;
            }

            if (!CommitPage(pageAddr)) {
                return 0;
            }

            page->protection = protection;
            page->isLargePage = true;
        }
    }

    m_stats.largePages += size / MMU_LARGE_PAGE_SIZE;

    m_nextAddress = baseAddress + size;
    UpdateStats();

    LOG_INFO("MMU", "Allocated large page: %llu bytes at 0x%llx", size, baseAddress);
    return baseAddress;
}

void MMU::FreeLargePage(uint64_t address) {
    if (address == 0) {
        return;
    }

    MemoryRegion* region = GetRegion(address);
    if (region) {
        FreeRange(region->baseAddress, region->size);
    }
}

void MMU::EnableCompression(bool enable) {
    m_compressionEnabled = enable;
    LOG_INFO("MMU", "Memory compression %s", enable ? "enabled" : "disabled");
}

uint64_t MMU::GetCompressedPageCount() const {
    return m_compressedPages.size();
}

void MMU::SetPageFaultHandler(std::function<void(uint64_t, bool)> handler) {
    m_pageFaultHandler = std::move(handler);
}

void MMU::TriggerPageFault(uint64_t virtualAddress, bool isWrite) {
    m_stats.pageFaults++;

    if (m_pageFaultHandler) {
        m_pageFaultHandler(virtualAddress, isWrite);
    } else {
        LOG_ERROR("MMU", "Page fault at 0x%llx (write=%d)", virtualAddress, isWrite ? 1 : 0);
    }
}

MMUStats MMU::GetStats() const {
    return m_stats;
}

void MMU::ResetStats() {
    m_stats = MMUStats{};
    m_stats.totalVirtualPages = m_totalPages;
    m_stats.totalPhysicalPages = m_totalPages;
}

uint64_t MMU::GetUsedMemory() const {
    uint64_t used = 0;
    for (const auto& page : m_pageTable) {
        if (page.isPresent) {
            used += MMU_PAGE_SIZE;
        }
    }
    return used;
}

uint64_t MMU::GetFreeMemory() const {
    return m_virtualSize - GetUsedMemory();
}

double MMU::GetUtilization() const {
    return static_cast<double>(GetUsedMemory()) / m_virtualSize * 100.0;
}

void MMU::DumpRegions() const {
    LOG_INFO("MMU", "Memory Regions:");
    for (const auto& region : m_regions) {
        LOG_INFO("MMU", "  0x%llx - 0x%llx (%llu bytes) prot=%d type=%d name=%s",
                 region.baseAddress,
                 region.baseAddress + region.size,
                 region.size,
                 static_cast<int>(region.protection),
                 static_cast<int>(region.type),
                 region.name.c_str());
    }
}

bool MMU::IsValidAddress(uint64_t address) const {
    return address < m_virtualSize;
}

MemoryRegion* MMU::GetRegion(uint64_t address) {
    auto it = std::find_if(m_regions.begin(), m_regions.end(),
                           [address](const MemoryRegion& region) {
                               return address >= region.baseAddress &&
                                      address < region.baseAddress + region.size;
                           });

    return (it != m_regions.end()) ? &(*it) : nullptr;
}

uint64_t MMU::AlignUp(uint64_t value, uint64_t alignment) const {
    return (value + alignment - 1) & ~(alignment - 1);
}

uint64_t MMU::AlignDown(uint64_t value, uint64_t alignment) const {
    return value & ~(alignment - 1);
}

uint32_t MMU::GetPageIndex(uint64_t address) const {
    return static_cast<uint32_t>(address / MMU_PAGE_SIZE);
}

uint64_t MMU::GetPageAddress(uint32_t pageIndex) const {
    return static_cast<uint64_t>(pageIndex) * MMU_PAGE_SIZE;
}

MemoryPage* MMU::GetPage(uint64_t virtualAddress) {
    if (virtualAddress >= m_virtualSize) {
        return nullptr;
    }

    uint32_t pageIndex = GetPageIndex(virtualAddress);
    if (pageIndex >= m_pageTable.size()) {
        return nullptr;
    }

    return &m_pageTable[pageIndex];
}

const MemoryPage* MMU::GetPage(uint64_t virtualAddress) const {
    return const_cast<MMU*>(this)->GetPage(virtualAddress);
}

void MMU::UpdateStats() {
    m_stats.usedVirtualPages = 0;
    m_stats.usedPhysicalPages = 0;

    for (const auto& page : m_pageTable) {
        if (page.isPresent) {
            m_stats.usedVirtualPages++;
            m_stats.usedPhysicalPages++;
        }
    }

    m_stats.compressedPages = m_compressedPages.size();
}

} // namespace Kyty::Common
