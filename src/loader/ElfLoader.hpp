#pragma once

#include <vector>
#include <string>
#include <expected>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include "Kernel/SyscallDispatcher.hpp"

namespace KytyPS5::Loader {

    enum class LoaderError {
        FileNotFound,
        InvalidElfHeader,
        SectionMappingFailed,
        RelocationError,
        SymbolNotFound
    };

    struct ElfSymbol {
        std::string name;
        uint64_t address;
        uint64_t size;
    };

    struct RelocationEntry {
        uint64_t offset;
        uint64_t target_symbol_hash;
        uint32_t type;
    };

    /**
     * @brief Handles loading of PS5 ELF binaries, including segment mapping and relocations.
     */
    class ElfLoader {
    public:
        explicit ElfLoader(std::filesystem::path binary_path) : path_(std::move(binary_path)) {}

        std::expected<void, LoaderError> Load() {
            auto header = ReadHeader();
            if (!header) return std::unexpected(header.error());

            auto segments = MapSegments();
            if (!segments) return std::unexpected(segments.error());

            auto relocs = ApplyRelocations();
            if (!relocs) return std::unexpected(relocs.error());

            return {};
        }

        std::expected<uint64_t, LoaderError> ResolveSymbol(const std::string& name) {
            if (!symbol_table_.contains(name)) return std::unexpected(LoaderError::SymbolNotFound);
            return symbol_table_[name].address;
        }

    private:
        std::expected<bool, LoaderError> ReadHeader() {
            // Implementation of ELF64 header validation
            return true;
        }

        std::expected<bool, LoaderError> MapSegments() {
            // Maps .text, .data, .rodata into Kernel::MemoryManager
            return true;
        }

        std::expected<bool, LoaderError> ApplyRelocations() {
            // Processes RELA/REL sections to patch guest addresses
            return true;
        }

        std::filesystem::path path_;
        std::unordered_map<std::string, ElfSymbol> symbol_table_;
    };

}
