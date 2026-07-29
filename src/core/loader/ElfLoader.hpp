#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <span>

namespace KytyPS5::Core {

    struct ElfHeader {
        uint32_t e_ident; // Simplified
        uint16_t e_type;
        uint16_t e_machine;
        uint32_t e_version;
        uint64_t e_entry;
        uint64_t e_phoff;
        uint64_t e_shoff;
        uint32_t e_flags;
        uint16_t e_ehhead;
        uint16_t e_phentsize;
        uint16_t e_phnum;
        uint16_t e_shentsize;
        uint16_t e_shnum;
        uint16_t e_shstrndx;
    };

    struct ElfProgramHeader {
        uint32_t p_type;
        uint32_t p_flags;
        uint64_t p_offset;
        uint64_t p_vaddr;
        uint64_t p_paddr;
        uint64_t p_filesz;
        uint64_t p_memsz;
        uint64_t p_align;
    };

    /**
     * @brief Handles loading and parsing of PS5 ELF binaries into the virtual memory space.
     */
    class ElfLoader {
    public:
        explicit ElfLoader(class VirtualMemoryManager& vmm) : vmm_(vmm) {}

        /**
         * @brief Loads an ELF file from disk and maps it into the VMM.
         * @return The entry point address of the loaded binary.
         */
        uint64_t LoadBinary(const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                throw std::runtime_error("Failed to open ELF binary: " + path.string());
            }

            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<uint8_t> buffer(fileSize);
            if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
                throw std::runtime_error("Failed to read ELF binary content.");
            }

            if (buffer.size() < sizeof(ElfHeader)) {
                throw std::runtime_error("File too small to be a valid ELF.");
            }

            const auto* header = reinterpret_cast<const ElfHeader*>(buffer.data());
            
            // Basic ELF Magic validation (simplified)
            if (header->e_ident != 0x464C457F) { // \x7fELF
                throw std::runtime_error("Invalid ELF magic number.");
            }

            const auto* phdr_base = reinterpret_cast<const ElfProgramHeader*>(buffer.data() + header->e_phoff);
            
            for (int i = 0; i < header->e_phnum; ++i) {
                const auto& phdr = phdr_base[i];
                
                // PT_LOAD segments only
                if (phdr.p_type == 1) { 
                    const uint8_t* data_ptr = buffer.data() + phdr.p_offset;
                    
                    // Map segment into VirtualMemoryManager
                    // We use the program header's vaddr as the destination
                    vmm_.MapRegion(phdr.p_vaddr, phdr.p_memsz, data_ptr, phdr.p_filesz);
                }
            }

            return header->e_entry;
        }

    private:
        VirtualMemoryManager& vmm_;
    };

}
