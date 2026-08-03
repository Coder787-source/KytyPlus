#include "core/jitOptimizer.h"
#include "common/logging/log.h"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <thread>

namespace Kyty::Core {

// Global optimizer instance
static JITOptimizer g_jitOptimizer;

JITOptimizer& GetJITOptimizer() {
    return g_jitOptimizer;
}

JITOptimizer::JITOptimizer() {
    m_stats = JITStats{};
}

JITOptimizer::~JITOptimizer() {
    Shutdown();
}

bool JITOptimizer::Initialize(const JITConfig& config) {
    std::lock_guard<std::mutex> lock(m_blockMutex);

    if (m_initialized) {
        LOGF("[JIT] WARNING: " "Already initialized");
        return true;
    }

    m_config = config;
    m_cacheSize = config.cacheSize;
    m_cacheMemory.resize(m_cacheSize);
    m_cacheUsed = 0;

    LOGF("[JIT] INFO: " "Initialized: Cache=%zu MB, OptLevel=%d, Linking=%d, SIMD=%d, PGO=%d",
             m_cacheSize / (1024 * 1024),
             static_cast<int>(config.optLevel),
             config.enableLinking ? 1 : 0,
             config.enableSIMD ? 1 : 0,
             config.enablePGO ? 1 : 0);

    m_initialized = true;
    ResetStats();

    return true;
}

void JITOptimizer::Shutdown() {
    std::lock_guard<std::mutex> lock(m_blockMutex);

    if (!m_initialized) {
        return;
    }

    ClearCache();
    m_cacheMemory.clear();
    m_compilationQueue.clear();
    m_executionTimes.clear();
    m_branchProbabilities.clear();

    m_initialized = false;

    LOGF("[JIT] INFO: " "Shutdown complete");
}

uint64_t JITOptimizer::CompileBlock(uint64_t guestAddress, const uint8_t* code, size_t size) {
    if (!m_initialized || !code || size == 0) {
        return 0;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Check if already compiled
    JITBlock* existingBlock = FindBlock(guestAddress);
    if (existingBlock && existingBlock->hostAddress != 0) {
        m_stats.cacheHits++;
        return existingBlock->hostAddress;
    }

    m_stats.cacheMisses++;
    m_stats.compilations++;

    // Create new block
    JITBlock* block = CreateBlock(guestAddress);
    if (!block) {
        LOGF("[JIT] ERROR: " "Failed to create block at 0x%llx", guestAddress);
        return 0;
    }

    block->guestAddress = guestAddress;
    block->guestSize = size;
    block->originalCode.assign(code, code + size);

    // Allocate cache memory
    size_t hostSize = size * 4; // Estimated expansion
    uint8_t* hostCode = AllocateCacheMemory(hostSize);
    if (!hostCode) {
        LOGF("[JIT] ERROR: " "Failed to allocate cache memory");
        DeleteBlock(block);
        return 0;
    }

    // In production: actual JIT compilation would happen here
    // For now, copy original code as placeholder
    std::memcpy(hostCode, code, std::min(size, hostSize));
    
    block->hostAddress = reinterpret_cast<uint64_t>(hostCode);
    block->hostSize = hostSize;
    block->isCached = true;
    block->executionCount = 0;

    // Apply optimizations based on level
    if (m_config.optLevel >= OptimizationLevel::Standard) {
        OptimizeBlock(*block);
    }

    if (m_config.enableLinking && m_config.optLevel >= OptimizationLevel::Standard) {
        // Linking would happen here
        block->isLinked = false; // Will be linked on first execution
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double compileTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    block->compileTime = compileTime;

    RecordCompilation(guestAddress, compileTime);

    LOGF("[JIT] DEBUG: " "Compiled block: guest=0x%llx host=0x%llx size=%zu time=%.2fms",
              guestAddress, block->hostAddress, size, compileTime);

    if (m_compilationCallback) {
        m_compilationCallback(*block);
    }

    return block->hostAddress;
}

uint64_t JITOptimizer::GetCompiledBlock(uint64_t guestAddress) {
    const JITBlock* block = FindBlock(guestAddress);
    return block ? block->hostAddress : 0;
}

void JITOptimizer::InvalidateBlock(uint64_t guestAddress) {
    std::lock_guard<std::mutex> lock(m_blockMutex);

    JITBlock* block = FindBlock(guestAddress);
    if (block) {
        DeleteBlock(block);
        m_stats.deoptimizations++;
        LOGF("[JIT] DEBUG: " "Invalidated block at 0x%llx", guestAddress);
    }
}

void JITOptimizer::InvalidateRange(uint64_t start, uint64_t end) {
    std::lock_guard<std::mutex> lock(m_blockMutex);

    std::vector<uint64_t> toInvalidate;
    for (const auto& [addr, block] : m_blocks) {
        if (addr >= start && addr < end) {
            toInvalidate.push_back(addr);
        }
    }

    for (uint64_t addr : toInvalidate) {
        JITBlock* block = FindBlock(addr);
        if (block) {
            DeleteBlock(block);
        }
    }

    LOGF("[JIT] INFO: " "Invalidated %zu blocks in range 0x%llx-0x%llx", toInvalidate.size(), start, end);
}

void JITOptimizer::ClearCache() {
    std::lock_guard<std::mutex> lock(m_blockMutex);

    for (auto& [addr, block] : m_blocks) {
        if (block->isCached && block->hostAddress != 0) {
            uint8_t* hostCode = reinterpret_cast<uint8_t*>(block->hostAddress);
            FreeCacheMemory(hostCode, block->hostSize);
        }
    }

    m_blocks.clear();
    m_cacheUsed = 0;
    m_stats = JITStats{};

    LOGF("[JIT] INFO: " "Cache cleared");
}

void JITOptimizer::RecordExecution(uint64_t guestAddress) {
    JITBlock* block = FindBlock(guestAddress);
    if (!block) {
        return;
    }

    block->executionCount++;

    // Check if block should be optimized
    if (!block->isOptimized && block->executionCount >= m_config.hotThreshold) {
        MarkBlockHot(guestAddress);
    }

    // Check if block should be deoptimized
    if (block->isOptimized && block->executionCount < m_config.coldThreshold) {
        MarkBlockCold(guestAddress);
    }

    if (m_executionCallback) {
        m_executionCallback(guestAddress, block->hostAddress);
    }
}

void JITOptimizer::RecordCompilation(uint64_t guestAddress, double timeMs) {
    // Update average compile time
    const double alpha = 0.1;
    m_stats.averageCompileTime = (1.0 - alpha) * m_stats.averageCompileTime + alpha * timeMs;
}

void JITOptimizer::MarkBlockHot(uint64_t guestAddress) {
    JITBlock* block = FindBlock(guestAddress);
    if (!block || block->isOptimized) {
        return;
    }

    LOGF("[JIT] INFO: " "Block 0x%llx is hot (%d executions), optimizing...", guestAddress, block->executionCount);

    if (m_config.enableSIMD && m_config.optLevel >= OptimizationLevel::Aggressive) {
        ApplySIMDOptimization(*block);
    }

    if (m_config.enablePGO && m_config.optLevel >= OptimizationLevel::Aggressive) {
        ApplyPGO(*block);
    }

    block->isOptimized = true;
    m_stats.optimizedBlocks++;
}

void JITOptimizer::MarkBlockCold(uint64_t guestAddress) {
    JITBlock* block = FindBlock(guestAddress);
    if (!block || !block->isOptimized) {
        return;
    }

    LOGF("[JIT] DEBUG: " "Block 0x%llx is cold, deoptimizing", guestAddress);

    // Reset optimization flags
    block->isOptimized = false;
    m_stats.optimizedBlocks--;
    m_stats.deoptimizations++;
}

bool JITOptimizer::OptimizeBlock(JITBlock& block) {
    // Basic optimization: remove redundant instructions
    // In production: full optimization pipeline
    
    LOGF("[JIT] DEBUG: " "Optimizing block at 0x%llx", block.guestAddress);
    
    // Placeholder for actual optimization
    return true;
}

bool JITOptimizer::LinkBlock(JITBlock& block, uint64_t targetAddress) {
    if (!m_config.enableLinking) {
        return false;
    }

    // Find target block
    JITBlock* targetBlock = FindBlock(targetAddress);
    if (!targetBlock) {
        return false;
    }

    // In production: patch jump instructions to directly link blocks
    // This eliminates dispatcher overhead for known branches

    block.isLinked = true;
    m_stats.linkedBlocks++;

    LOGF("[JIT] DEBUG: " "Linked block 0x%llx -> 0x%llx", block.guestAddress, targetAddress);
    return true;
}

bool JITOptimizer::UnlinkBlock(JITBlock& block) {
    if (!block.isLinked) {
        return false;
    }

    // In production: restore original jump instructions
    block.isLinked = false;
    m_stats.linkedBlocks--;

    return true;
}

void JITOptimizer::ApplyPGO(JITBlock& block) {
    if (!m_config.enablePGO) {
        return;
    }

    auto it = m_executionTimes.find(block.guestAddress);
    if (it == m_executionTimes.end()) {
        return;
    }

    const auto& times = it->second;
    if (times.empty()) {
        return;
    }

    // Calculate average execution time
    double avgTime = 0.0;
    for (double t : times) {
        avgTime += t;
    }
    avgTime /= times.size();

    block.execTime = avgTime;

    // Apply optimizations based on profile
    auto branchIt = m_branchProbabilities.find(block.guestAddress);
    if (branchIt != m_branchProbabilities.end()) {
        // Optimize for most likely branches
        LOGF("[JIT] DEBUG: " "PGO: Block 0x%llx avg time=%.3fms", block.guestAddress, avgTime);
    }
}

bool JITOptimizer::ApplySIMDOptimization(JITBlock& block) {
    if (!m_config.enableSIMD) {
        return false;
    }

    LOGF("[JIT] DEBUG: " "Applying SIMD optimization to block 0x%llx", block.guestAddress);
    
    // In production: vectorization analysis and transformation
    return true;
}

bool JITOptimizer::VectorizeLoop(JITBlock& block) {
    // Loop vectorization
    // In production: analyze loop structure, apply SIMD
    
    return false; // Stub
}

void JITOptimizer::QueueCompilation(uint64_t guestAddress, const uint8_t* code, size_t size) {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    m_compilationQueue.emplace_back(guestAddress, std::vector<uint8_t>(code, code + size));
    m_pendingCompilations++;

    LOGF("[JIT] DEBUG: " "Queued compilation for 0x%llx (queue size=%zu)", guestAddress, m_compilationQueue.size());
}

void JITOptimizer::ProcessCompilationQueue() {
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> queue;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_compilationQueue.empty()) {
            return;
        }
        queue = std::move(m_compilationQueue);
        m_compilationQueue.clear();
    }

    for (auto& [addr, code] : queue) {
        CompileBlock(addr, code.data(), code.size());
        m_pendingCompilations--;
    }
}

int32_t JITOptimizer::GetPendingCompilationCount() const {
    return m_pendingCompilations.load();
}

void JITOptimizer::AllocateRegisters(JITBlock& block) {
    // Register allocation
    // In production: graph coloring or linear scan
    
    LOGF("[JIT] DEBUG: " "Allocating registers for block 0x%llx", block.guestAddress);
}

void JITOptimizer::SpillRegisters(JITBlock& block) {
    // Register spilling
    // In production: spill to stack when registers exhausted
    
    LOGF("[JIT] DEBUG: " "Spilling registers for block 0x%llx", block.guestAddress);
}

void JITOptimizer::SetCompilationCallback(CompilationCallback callback) {
    m_compilationCallback = std::move(callback);
}

void JITOptimizer::SetExecutionCallback(ExecutionCallback callback) {
    m_executionCallback = std::move(callback);
}

JITStats JITOptimizer::GetStats() const {
    return m_stats;
}

JITConfig JITOptimizer::GetConfig() const {
    return m_config;
}

void JITOptimizer::ResetStats() {
    m_stats = JITStats{};
    m_stats.cacheSize = m_cacheSize;
}

double JITOptimizer::GetSpeedup() const {
    if (m_stats.averageExecTime == 0 || m_stats.averageCompileTime == 0) {
        return 1.0;
    }
    return m_stats.averageCompileTime / m_stats.averageExecTime;
}

double JITOptimizer::GetCacheHitRate() const {
    uint64_t total = m_stats.cacheHits + m_stats.cacheMisses;
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(m_stats.cacheHits) / total * 100.0;
}

size_t JITOptimizer::GetCacheUsage() const {
    return m_cacheUsed;
}

size_t JITOptimizer::GetCacheFree() const {
    return m_cacheSize - m_cacheUsed;
}

double JITOptimizer::GetCacheUtilization() const {
    return static_cast<double>(m_cacheUsed) / m_cacheSize * 100.0;
}

void JITOptimizer::DefragmentCache() {
    std::lock_guard<std::mutex> lock(m_blockMutex);

    LOGF("[JIT] INFO: " "Defragmenting cache...");
    
    // In production: compact cache, update pointers
    // For now, just log
}

void JITOptimizer::DumpBlock(uint64_t guestAddress) const {
    const JITBlock* block = FindBlock(guestAddress);
    if (!block) {
        LOGF("[JIT] INFO: " "Block 0x%llx not found", guestAddress);
        return;
    }

    LOGF("[JIT] INFO: " "Block 0x%llx:", guestAddress);
    LOGF("[JIT] INFO: " "  Host: 0x%llx", block->hostAddress);
    LOGF("[JIT] INFO: " "  Guest Size: %zu", block->guestSize);
    LOGF("[JIT] INFO: " "  Host Size: %zu", block->hostSize);
    LOGF("[JIT] INFO: " "  Exec Count: %d", block->executionCount);
    LOGF("[JIT] INFO: " "  Optimized: %d", block->isOptimized ? 1 : 0);
    LOGF("[JIT] INFO: " "  Linked: %d", block->isLinked ? 1 : 0);
    LOGF("[JIT] INFO: " "  Cached: %d", block->isCached ? 1 : 0);
    LOGF("[JIT] INFO: " "  Compile Time: %.2f ms", block->compileTime);
    LOGF("[JIT] INFO: " "  Exec Time: %.2f ms", block->execTime);
}

void JITOptimizer::DumpStats() const {
    LOGF("[JIT] INFO: " "=== JIT Statistics ===");
    LOGF("[JIT] INFO: " "Total Blocks: %llu", m_stats.totalBlocks);
    LOGF("[JIT] INFO: " "Cached Blocks: %llu", m_stats.cachedBlocks);
    LOGF("[JIT] INFO: " "Optimized Blocks: %llu", m_stats.optimizedBlocks);
    LOGF("[JIT] INFO: " "Linked Blocks: %llu", m_stats.linkedBlocks);
    LOGF("[JIT] INFO: " "Cache Hits: %llu", m_stats.cacheHits);
    LOGF("[JIT] INFO: " "Cache Misses: %llu", m_stats.cacheMisses);
    LOGF("[JIT] INFO: " "Hit Rate: %.1f%%", GetCacheHitRate());
    LOGF("[JIT] INFO: " "Compilations: %llu", m_stats.compilations);
    LOGF("[JIT] INFO: " "Deoptimizations: %llu", m_stats.deoptimizations);
    LOGF("[JIT] INFO: " "Avg Compile Time: %.2f ms", m_stats.averageCompileTime);
    LOGF("[JIT] INFO: " "Avg Exec Time: %.2f ms", m_stats.averageExecTime);
    LOGF("[JIT] INFO: " "Speedup: %.2fx", GetSpeedup());
    LOGF("[JIT] INFO: " "Cache Usage: %zu / %zu (%.1f%%)", m_cacheUsed, m_cacheSize, GetCacheUtilization());
    LOGF("[JIT] INFO: " "======================");
}

bool JITOptimizer::IsBlockOptimized(uint64_t guestAddress) const {
    const JITBlock* block = FindBlock(guestAddress);
    return block && block->isOptimized;
}

bool JITOptimizer::IsBlockCached(uint64_t guestAddress) const {
    const JITBlock* block = FindBlock(guestAddress);
    return block && block->isCached;
}

JITBlock* JITOptimizer::FindBlock(uint64_t guestAddress) {
    auto it = m_blocks.find(guestAddress);
    return (it != m_blocks.end()) ? it->second.get() : nullptr;
}

const JITBlock* JITOptimizer::FindBlock(uint64_t guestAddress) const {
    auto it = m_blocks.find(guestAddress);
    return (it != m_blocks.end()) ? it->second.get() : nullptr;
}

JITBlock* JITOptimizer::CreateBlock(uint64_t guestAddress) {
    auto block = std::make_unique<JITBlock>();
    JITBlock* rawPtr = block.get();
    m_blocks[guestAddress] = std::move(block);
    m_stats.totalBlocks++;
    return rawPtr;
}

void JITOptimizer::DeleteBlock(JITBlock* block) {
    if (!block) {
        return;
    }

    if (block->isCached && block->hostAddress != 0) {
        uint8_t* hostCode = reinterpret_cast<uint8_t*>(block->hostAddress);
        FreeCacheMemory(hostCode, block->hostSize);
    }

    uint64_t addr = block->guestAddress;
    m_blocks.erase(addr);
    m_stats.totalBlocks--;
}

void JITOptimizer::UpdateStats() {
    m_stats.cachedBlocks = 0;
    m_stats.optimizedBlocks = 0;
    m_stats.linkedBlocks = 0;

    for (const auto& [addr, block] : m_blocks) {
        if (block->isCached) m_stats.cachedBlocks++;
        if (block->isOptimized) m_stats.optimizedBlocks++;
        if (block->isLinked) m_stats.linkedBlocks++;
    }

    m_stats.cacheUsage = m_cacheUsed;
}

uint8_t* JITOptimizer::AllocateCacheMemory(size_t size) {
    if (m_cacheUsed + size > m_cacheSize) {
        // Cache full - would need eviction policy
        LOGF("[JIT] WARNING: " "Cache full, cannot allocate %zu bytes", size);
        return nullptr;
    }

    uint8_t* ptr = m_cacheMemory.data() + m_cacheUsed;
    m_cacheUsed += size;
    return ptr;
}

void JITOptimizer::FreeCacheMemory(uint8_t* ptr, size_t size) {
    // Simple bump allocator - doesn't actually free
    // In production: use proper memory management
    if (ptr >= m_cacheMemory.data() && ptr < m_cacheMemory.data() + m_cacheSize) {
        // Would decrement m_cacheUsed in a real implementation
    }
}

} // namespace Kyty::Core
