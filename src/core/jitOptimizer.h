#pragma once

#include "common/common.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

namespace Kyty::Core {

// JIT Optimization Framework
// Provides block caching, linking, register allocation, and PGO

constexpr size_t JIT_CACHE_SIZE = 128 * 1024 * 1024; // 128MB
constexpr size_t JIT_MAX_BLOCKS = 100000;
constexpr int32_t JIT_HOT_THRESHOLD = 100; // Executions before optimization
constexpr int32_t JIT_COLD_THRESHOLD = 10; // Executions before deoptimization

enum class OptimizationLevel {
    None,      // No optimization
    Basic,     // Basic block caching
    Standard,  // + linking, simple opts
    Aggressive // + PGO, SIMD, multi-threaded
};

struct JITBlock {
    uint64_t guestAddress = 0;
    uint64_t hostAddress = 0;
    size_t guestSize = 0;
    size_t hostSize = 0;
    int32_t executionCount = 0;
    bool isOptimized = false;
    bool isLinked = false;
    bool isCached = false;
    double compileTime = 0.0; // milliseconds
    double execTime = 0.0; // milliseconds (average)
    
    std::vector<uint8_t> machineCode;
    std::vector<uint8_t> originalCode;
};

struct JITStats {
    uint64_t totalBlocks = 0;
    uint64_t cachedBlocks = 0;
    uint64_t optimizedBlocks = 0;
    uint64_t linkedBlocks = 0;
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    uint64_t compilations = 0;
    uint64_t deoptimizations = 0;
    double averageCompileTime = 0.0;
    double averageExecTime = 0.0;
    double speedup = 1.0;
    size_t cacheUsage = 0;
    size_t cacheSize = 0;
};

struct JITConfig {
    OptimizationLevel optLevel = OptimizationLevel::Standard;
    size_t cacheSize = JIT_CACHE_SIZE;
    int32_t hotThreshold = JIT_HOT_THRESHOLD;
    int32_t coldThreshold = JIT_COLD_THRESHOLD;
    bool enableLinking = true;
    bool enableSIMD = true;
    bool enableMultiThread = true;
    bool enablePGO = true;
    bool enableDebugInfo = false;
};

using CompilationCallback = std::function<void(const JITBlock&)>;
using ExecutionCallback = std::function<void(uint64_t guestAddr, uint64_t hostAddr)>;

class JITOptimizer {
public:
    JITOptimizer();
    ~JITOptimizer();

    // Initialization
    bool Initialize(const JITConfig& config = JITConfig());
    void Shutdown();

    // Block management
    uint64_t CompileBlock(uint64_t guestAddress, const uint8_t* code, size_t size);
    uint64_t GetCompiledBlock(uint64_t guestAddress);
    void InvalidateBlock(uint64_t guestAddress);
    void InvalidateRange(uint64_t start, uint64_t end);
    void ClearCache();

    // Execution tracking
    void RecordExecution(uint64_t guestAddress);
    void RecordCompilation(uint64_t guestAddress, double timeMs);
    void MarkBlockHot(uint64_t guestAddress);
    void MarkBlockCold(uint64_t guestAddress);

    // Optimization
    bool OptimizeBlock(JITBlock& block);
    bool LinkBlock(JITBlock& block, uint64_t targetAddress);
    bool UnlinkBlock(JITBlock& block);
    void ApplyPGO(JITBlock& block);

    // SIMD optimization
    bool ApplySIMDOptimization(JITBlock& block);
    bool VectorizeLoop(JITBlock& block);

    // Multi-threaded compilation
    void QueueCompilation(uint64_t guestAddress, const uint8_t* code, size_t size);
    void ProcessCompilationQueue();
    int32_t GetPendingCompilationCount() const;

    // Register allocation
    void AllocateRegisters(JITBlock& block);
    void SpillRegisters(JITBlock& block);

    // Callbacks
    void SetCompilationCallback(CompilationCallback callback);
    void SetExecutionCallback(ExecutionCallback callback);

    // Statistics
    JITStats GetStats() const { return m_stats; }
    JITConfig GetConfig() const { return m_config; }
    void ResetStats();
    double GetSpeedup() const;
    double GetCacheHitRate() const;

    // Cache management
    size_t GetCacheUsage() const;
    size_t GetCacheFree() const;
    double GetCacheUtilization() const;
    void DefragmentCache();

    // Debugging
    void DumpBlock(uint64_t guestAddress) const;
    void DumpStats() const;
    bool IsBlockOptimized(uint64_t guestAddress) const;
    bool IsBlockCached(uint64_t guestAddress) const;

private:
    JITBlock* FindBlock(uint64_t guestAddress);
    const JITBlock* FindBlock(uint64_t guestAddress) const;
    JITBlock* CreateBlock(uint64_t guestAddress);
    void DeleteBlock(JITBlock* block);
    void UpdateStats();
    uint8_t* AllocateCacheMemory(size_t size);
    void FreeCacheMemory(uint8_t* ptr, size_t size);

    bool m_initialized = false;
    JITConfig m_config;
    JITStats m_stats;

    std::unordered_map<uint64_t, std::unique_ptr<JITBlock>> m_blocks;
    std::vector<uint8_t> m_cacheMemory;
    size_t m_cacheUsed = 0;

    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> m_compilationQueue;
    std::mutex m_queueMutex;
    std::atomic<int32_t> m_pendingCompilations{0};

    std::mutex m_blockMutex;

    CompilationCallback m_compilationCallback;
    ExecutionCallback m_executionCallback;

    // PGO data
    std::unordered_map<uint64_t, std::vector<double>> m_executionTimes;
    std::unordered_map<uint64_t, int32_t> m_branchProbabilities;
};

// Global optimizer instance
JITOptimizer& GetJITOptimizer();

} // namespace Kyty::Core
