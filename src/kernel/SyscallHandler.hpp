#pragma once

#include <cstdint>
#include <unordered_map>
#include <functional>
#include <span>
#include <vector>
#include <iostream>
#include \"Core\\JitDispatcher.hpp\"\n#include \"ScePadDispatcher.hpp\"\n#include \"AstroCompatLayer.hpp\"

namespace KytyPS5::Kernel {

    class SyscallHandler {
    public:
        using SyscallFunc = std::function<uint64_t(Core::ThreadContext&, std::span<uint64_t>)>;

        SyscallHandler(ScePadDispatcher* pad_dispatcher, AstroCompatLayer* compat_layer) 
            : m_pad_dispatcher(pad_dispatcher), m_compat_layer(compat_layer) {
            InitializeSyscallTable();
        }

        uint64_t HandleSyscall(uint32_t call_id, Core::ThreadContext& ctx) {
            if (auto it = m_table.find(call_id); it != m_table.end()) {
                // Arguments are passed in X0-X7 per ARM64 ABI
                std::vector<uint64_t> args(8);
                for (int i = 0; i < 8; ++i) {
                    args[i] = ctx.gprs[i];
                }
                return it->second(ctx, args);
            }
            
            std::cerr << \"[KERNEL] Unimplemented Syscall ID: 0x\" << std::hex << call_id << std::endl;
            return 0xDEADBEEF;
        }

    private:
        std::unordered_map<uint32_t, SyscallFunc> m_table;
        ScePadDispatcher* m_pad_dispatcher;
        AstroCompatLayer* m_compat_layer;

        void InitializeSyscallTable() {
            // SceKernel Memory Mapping (Wrapped by AstroCompatLayer for commercial stability)
            m_table[0x1001] = [this](Core::ThreadContext& ctx, auto args) { 
                return m_compat_layer ? m_compat_layer->HandleMemMap(args) : this->SceKernelMemMap(ctx, args); 
            };

            // ScePad Input Dispatch (Routed through ScePadDispatcher to prevent input lag crashes)
            m_table[0x4001] = [this](Core::ThreadContext& ctx, auto args) {
                return m_pad_dispatcher ? m_pad_dispatcher->Dispatch(ctx, args) : 0;
            };

            // SceKernel Thread Create
            m_table[0x2005] = [this](Core::ThreadContext& ctx, auto args) { return this->SceKernelThreadCreate(ctx, args); };
            // SceKernel Event Wait
            m_table[0x300A] = [this](Core::ThreadContext& ctx, auto args) { return this->SceKernelEventWait(ctx, args); };
        }

        uint64_t SceKernelMemMap(Core::ThreadContext& ctx, std::span<uint64_t> args) {
            uint64_t addr = args[0];
            uint64_t size = args[1];
            uint64_t flags = args[2];
            return 0; 
        }

        uint64_t SceKernelThreadCreate(Core::ThreadContext& ctx, std::span<uint64_t> args) {
            return 0;
        }

        uint64_t SceKernelEventWait(Core::ThreadContext& ctx, std::span<uint64_t> args) {
            return 0;
        }
    };

}
