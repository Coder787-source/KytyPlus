#include "orchestrator.h"
#include <iostream>

namespace kyty {

void SystemOrchestrator::RunTest() {
    auto provider = std::make_unique<ElfLoader>();
    auto jit = std::make_unique<JitTranslator>();
    auto gpu = std::make_unique<GpuDevice>();
    
    SystemOrchestrator orchestrator(std::move(provider), std::move(jit), std::move(gpu));
    
    auto result = orchestrator.Boot("test_homebrew.elf");
    if (result) {
        std::cout << "Boot sequence verified successfully.\n";
    } else {
        std::cerr << "Boot sequence failed.\n";
    }
}

} // namespace kyty
