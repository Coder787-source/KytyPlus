#pragma once

#include "core/ICache.hpp"
#include "core/JitDispatcher.hpp"
#include <vector>
#include <memory>

namespace KytyPS5::Core {

    /**
     * @brief High-level JIT Execution Engine that bridges the Dispatcher and the ICache.
     */
    class JitEngine {
    public:
        JitEngine() : icache_(std::make_unique<ICache>()), dispatcher_(std::make_unique<JitDispatcher>()) {}

        /**
         * @brief Main entry point for guest code execution.
         * @param pc Current Guest Program Counter.
         * @param instructions Block of guest instructions to execute.
         */
        void Execute(GuestAddr pc, const std::vector<Instruction>& instructions) {
            // 1. Fast Path: Try to find existing translated code
            HostAddr cached_code = icache_->Lookup(pc);
            if (cached_code) {
                ExecuteNative(cached_code);
                return;
            }

            // 2. Slow Path: Translate block and cache it
            std::vector<uint8_t> native_blob = dispatcher_->TranslateBlock(instructions);
            HostAddr native_ptr = icache_->Insert(pc, native_blob);
            
            ExecuteNative(native_ptr);
        }

    private:
        void ExecuteNative(HostAddr addr) {
            // Cast the host address to a function pointer and execute
            using NativeFunc = void (*)();
            NativeFunc func = reinterpret_cast<NativeFunc>(addr);
            func();
        }

        std::unique_ptr<ICache> icache_;
        std::unique_ptr<JitDispatcher> dispatcher_;
    };

}
