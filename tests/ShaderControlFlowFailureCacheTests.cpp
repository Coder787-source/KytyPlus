// SPDX-License-Identifier: MIT
#include "graphics/shader/shaderControlFlowFailureCache.h"
#include <array>
#include <cstdio>
#include <thread>

int main() {
    using Libs::Graphics::ShaderControlFlowFailureCache;
    ShaderControlFlowFailureCache cache;
    const std::array<uint32_t, 3> code {0x1234, 0xbf920001, 0xbf810000};
    const std::array<uint32_t, 3> replacement {0x1234, 0xbf800000, 0xbf810000};
    const auto error = "unsupported control-flow instruction in CFG at pc 0x00000004: SOPP";
    int failures = 0;
    const auto check = [&](bool pass, const char* label) {
        std::printf("%s: %s\n", pass ? "PASS" : "FAIL", label);
        failures += !pass;
    };
    check(!cache.Contains(1, code), "cache miss before rejection");
    check(!cache.Remember(1, code, "buffer descriptor unavailable"), "resource failures remain retryable");
    check(!cache.Remember(1, code, "SPIR-V validation failed"), "validation failures remain retryable");
    check(cache.Remember(1, code, error), "deterministic CFG rejection recorded");
    check(cache.Contains(1, code), "identical shader rejection reused");
    check(!cache.Contains(2, code), "different shader address not rejected");
    check(!cache.Contains(1, replacement), "same-address modified code invalidates rejection");
    check(!cache.Contains(1, code), "old entry removed after mutation");
    check(!cache.Remember(1, {}, error), "empty code never cached");
    bool thread_ok = false;
    std::thread worker([&] { thread_ok = cache.Remember(4, code, error) && cache.Contains(4, code); });
    worker.join();
    check(thread_ok, "cache safely used from another thread");
    const std::array<uint32_t, 2> layout {64, 8}, changed_layout {32, 8};
    const auto provenance = "shader resource tracking: hash=0x1 stage=compute pc=0x4 descriptor source 2 dword 0 contains an unknown value 1 (unknown)";
    check(cache.Remember(5, code, provenance, layout), "static SSA provenance failure recorded");
    check(cache.Contains(5, code, layout), "same code and stage layout reuse rejection");
    check(!cache.Contains(5, code, changed_layout), "stage layout change retries compilation");
    check(!cache.Remember(5, code, "shader resource tracking: descriptor binding missing", layout), "other resource failures not cached");
    for (uint64_t address = 10; address < 150; ++address) cache.Remember(address, code, error);
    check(cache.Contains(149, code), "bounded cache accepts new failures after eviction");
    return failures ? 1 : 0;
}
