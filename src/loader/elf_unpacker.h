#ifndef KYTY_ELF_UNPACKER_H
#define KYTY_ELF_UNPACKER_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "binary_decryption.h"

namespace Emulator {

/** Scaffolding-only allocator; not Libs::LibKernel::Memory. */
class StubMemoryManager {
public:
	void* Allocate(size_t /*size*/) { return nullptr; }
};

struct ElfSection {
	std::string name;
	uint64_t virtual_address = 0;
	size_t size = 0;
	std::vector<uint8_t> data;
};

class ElfUnpacker {
public:
	explicit ElfUnpacker(StubMemoryManager* mem_manager) : mem_manager_(mem_manager) {}

	bool MapBinaryToMemory(const std::vector<uint8_t>& decrypted_data) {
		std::cout << "[Loader] Unpacking ELF binary... Size: " << decrypted_data.size()
		          << " bytes" << std::endl;
		if (decrypted_data.size() < 64) {
			std::cerr << "[Loader] Invalid ELF: Binary too small." << std::endl;
			return false;
		}
		if (!mem_manager_) {
			return false;
		}
		(void)mem_manager_->Allocate(0x1000);
		return true;
	}

private:
	StubMemoryManager* mem_manager_;
};

} // namespace Emulator

#endif // KYTY_ELF_UNPACKER_H
