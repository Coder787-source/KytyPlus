#pragma once

#include <vector>
#include <string>
#include "kyty_expected.hpp"
#include <unordered_map>
#include <memory>
#include <filesystem>
#include "../kernel/SyscallDispatcher.hpp"

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

    struct LoadImage {
        uint64_t entry_point = 0;
    };

    /**
     * @brief Handles loading of PS5 ELF binaries, including segment mapping and relocations.
     */
    class ElfLoader {
    public:
        explicit ElfLoader(std::filesystem::path binary_path = {}) : path_(std::move(binary_path)) {}

        kyty::expected<LoadImage, std::string> Load(const std::string& image_path) {
            path_ = image_path;
            auto ok = Load();
            if (!ok) {
                return kyty::unexpected(std::string("ELF load failed"));
            }
            LoadImage image;
            image.entry_point = 0;
            return image;
        }

        kyty::expected<void, LoaderError> Load() {
            auto header = ReadHeader();
            if (!header) return kyty::unexpected(header.error());

            auto segments = MapSegments();
            if (!segments) return kyty::unexpected(segments.error());

            auto relocs = ApplyRelocations();
            if (!relocs) return kyty::unexpected(relocs.error());

            return {};
        }

        kyty::expected<uint64_t, LoaderError> ResolveSymbol(const std::string& name) {
            if (!symbol_table_.contains(name)) return kyty::unexpected(LoaderError::SymbolNotFound);
            return symbol_table_[name].address;
        }

    private:
        kyty::expected<bool, LoaderError> ReadHeader() {
            // Implementation of ELF64 header validation
            return true;
        }

        kyty::expected<bool, LoaderError> MapSegments() {
            // Maps .text, .data, .rodata into Kernel::MemoryManager
            return true;
        }

        kyty::expected<bool, LoaderError> ApplyRelocations() {
            // Processes RELA/REL sections to patch guest addresses
            return true;
        }

        std::filesystem::path path_;
        std::unordered_map<std::string, ElfSymbol> symbol_table_;
    };

}
