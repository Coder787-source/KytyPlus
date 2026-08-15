#include "common/ps5_nvme_lle.h"

#include "common/mmuVirtualMemory.h"

namespace Common {

void PS5NvmeLleDevice::ReadBlockPhys(uint64_t phys_addr, void* dest, size_t len) {
	// Route through the real MMU's physical memory. The MMU exposes a typed
	// ReadBlock; we use the global MMU instance.
	auto& mmu = Kyty::Common::GetMMU();
	mmu.ReadBlock(phys_addr, dest, static_cast<uint64_t>(len));
}

void PS5NvmeLleDevice::WriteBlockPhys(uint64_t phys_addr, const void* src, size_t len) {
	auto& mmu = Kyty::Common::GetMMU();
	mmu.WriteBlock(phys_addr, src, static_cast<uint64_t>(len));
}

} // namespace Common