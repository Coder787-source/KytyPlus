#include "firmware/pupParser.h"
#include "common/logging/log.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Libs::Firmware {

// Known PS5 firmware file IDs (partial list from reverse engineering)
// NOTE: These are educated guesses based on PS4 patterns. Real PS5 IDs may differ.
struct KnownFileId {
    uint64_t id;
    const char* name;
    bool is_prx;
};

static constexpr KnownFileId kKnownFiles[] = {
    // System modules
    {0x100, "ps5firmware.pkg", false},
    {0x200, "ps5recovery.dat", false},
    {0x300, "version.txt", false},

    // Kernel and core libraries
    {0x1000, "kernel.prx", true},
    {0x1001, "libkernel.prx", true},
    {0x1002, "libkernel_sys.prx", true},

    // System libraries
    {0x2000, "libSceBase.prx", true},
    {0x2001, "libSceLibcInternal.prx", true},
    {0x2002, "libSceFile.prx", true},
    {0x2003, "libSceNet.prx", true},
    {0x2004, "libSceNp.prx", true},
    {0x2005, "libScePad.prx", true},
    {0x2006, "libSceAudio.prx", true},
    {0x2007, "libSceVideoOut.prx", true},
    {0x2008, "libSceGnmDriver.prx", true},
    {0x2009, "libSceSaveData.prx", true},
    {0x200A, "libSceSystemService.prx", true},
    {0x200B, "libSceUserService.prx", true},
    {0x200C, "libSceCommonDialog.prx", true},

    // Graphics and compute
    {0x3000, "libSceGpu.prx", true},
    {0x3001, "libSceGpuAsync.prx", true},
    {0x3002, "libSceGnmCompute.prx", true},

    // Audio
    {0x4000, "libSceAudio2.prx", true},
    {0x4001, "libSceAudioOut.prx", true},
    {0x4002, "libSceAudioIn.prx", true},
    {0x4003, "libSceAudio3d.prx", true},

    // Network
    {0x5000, "libSceHttp.prx", true},
    {0x5001, "libSceSsl.prx", true},
    {0x5002, "libSceNetCtl.prx", true},

    // Input
    {0x6000, "libScePadTrigger.prx", true},
    {0x6001, "libSceTouch.prx", true},
    {0x6002, "libSceMotion.prx", true},

    // Storage
    {0x7000, "libSceSaveData.prx", true},
    {0x7001, "libSceAppInst.prx", true},
};

PupParseResult PupParser::Parse(const std::string& pup_path) {
    PupParseResult result {};

    // Open file
    std::ifstream file(pup_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.ok = false;
        result.error = "Failed to open PUP file: " + pup_path;
        return result;
    }

    const auto file_size = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    LOGF("[Firmware] INFO: Parsing PUP file (%.2f MB)\n",
         static_cast<double>(file_size) / (1024.0 * 1024.0));

    // Read and validate header
    // Try to autodetect PS4 vs PS5 format by reading raw bytes first
    std::vector<uint8_t> raw_header(64);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(raw_header.data()), raw_header.size());
    if (!file.good()) {
        result.ok = false;
        result.error = "Failed to read PUP header";
        return result;
    }

    // Check magic number (first 4 bytes)
    const uint32_t magic = *reinterpret_cast<const uint32_t*>(raw_header.data());
    LOGF("[Firmware] DEBUG: Raw magic=0x%08X\n", magic);

    // PS5 might use a different magic or header layout
    // For now, accept the standard magic but log if different
    if (magic != 0x70757000) {
        LOGF("[Firmware] WARN: Unexpected magic 0x%08X (expected 0x70757000)\n", magic);
        LOGF("[Firmware] INFO: This may be a PS5-specific format variation\n");
    }

    // Parse as standard header (will adjust if PS5 format differs)
    PupHeader header {};
    std::memcpy(&header, raw_header.data(), sizeof(header));

    // Debug: print raw header for format discovery
    LOGF("[Firmware] DEBUG: PUP magic=0x%08X\n", header.magic);
    LOGF("[Firmware] DEBUG: PUP version=%u\n", header.version);
    LOGF("[Firmware] DEBUG: PUP file_size=%llu (actual=%llu)\n",
         static_cast<unsigned long long>(header.file_size),
         static_cast<unsigned long long>(file_size));
    LOGF("[Firmware] DEBUG: PUP num_files=%u\n", header.num_files);
    LOGF("[Firmware] DEBUG: PUP flags=0x%08X\n", header.flags);
    LOGF("[Firmware] DEBUG: PUP header_hash_offset=0x%llX\n",
         static_cast<unsigned long long>(header.header_hash_offset));
    LOGF("[Firmware] DEBUG: PUP data_hash_offset=0x%llX\n",
         static_cast<unsigned long long>(header.data_hash_offset));

    if (!ValidateHeader(header, file_size)) {
        result.ok = false;
        result.error = "Invalid PUP header (magic=" +
                       std::to_string(header.magic) + ", expected 0x70757000)";
        return result;
    }

    result.header = header;

    LOGF("[Firmware] INFO: PUP version=%u, files=%u, flags=0x%08X\n",
         header.version, header.num_files, header.flags);

    // Read file entries
    const uint64_t entries_offset = sizeof(PupHeader);
    file.seekg(entries_offset, std::ios::beg);

    uint32_t prx_count = 0;
    uint32_t elf_count = 0;

    for (uint32_t i = 0; i < header.num_files; i++) {
        // Read entry - PS5 might use different size than PS4
        // Standard entry is 112 bytes, but log if it differs
        std::vector<uint8_t> raw_entry(sizeof(PupFileEntry));
        file.read(reinterpret_cast<char*>(raw_entry.data()), raw_entry.size());
        if (!file.good()) {
            result.ok = false;
            result.error = "Failed to read file entry " + std::to_string(i);
            return result;
        }

        PupFileEntry entry {};
        std::memcpy(&entry, raw_entry.data(), sizeof(entry));

        // Debug: print first 20 entries for format discovery
        if (i < 20 || IsPrxModule(entry.file_id)) {
            LOGF("[Firmware] DEBUG: Entry %u: ID=0x%08llX, Offset=0x%llX, Size=%llu bytes, Flags=0x%llX\n",
                 i,
                 static_cast<unsigned long long>(entry.file_id),
                 static_cast<unsigned long long>(entry.offset),
                 static_cast<unsigned long long>(entry.size),
                 static_cast<unsigned long long>(entry.flags));
        }

        result.entries.push_back(entry);

        // Read file data
        file.seekg(entry.offset, std::ios::beg);
        std::vector<uint8_t> data(static_cast<size_t>(entry.size));
        file.read(reinterpret_cast<char*>(data.data()), entry.size);
        if (!file.good()) {
            result.ok = false;
            result.error = "Failed to read file data for entry " + std::to_string(i);
            return result;
        }

        // Verify data integrity (check ELF magic)
        const bool has_elf_magic = (data.size() >= 4 &&
                                   data[0] == 0x7F && data[1] == 'E' &&
                                   data[2] == 'L' && data[3] == 'F');

        // Check if this is a PRX module
        const bool is_prx = IsPrxData(data);

        if (has_elf_magic && !is_prx) {
            LOGF("[Firmware] WARN: Entry 0x%08llX has ELF magic but failed PRX validation\n",
                 static_cast<unsigned long long>(entry.file_id));
        }

        FirmwareModule module {};
        module.file_id = entry.file_id;
        module.data = std::move(data);
        module.is_prx = is_prx;
        module.is_valid = is_prx || has_elf_magic; // Accept any valid ELF
        module.name = ExtractModuleName("", entry.file_id);
        module.path = ""; // Will be set during extraction

        if (is_prx) {
            prx_count++;
            LOGF("[Firmware] INFO: Found PRX module: %s (0x%08llX, %llu bytes)\n",
                 module.name.c_str(),
                 static_cast<unsigned long long>(entry.file_id),
                 static_cast<unsigned long long>(module.data.size()));
        } else if (has_elf_magic) {
            elf_count++;
            LOGF("[Firmware] INFO: Found ELF file (non-PRX): 0x%08llX (%llu bytes)\n",
                 static_cast<unsigned long long>(entry.file_id),
                 static_cast<unsigned long long>(module.data.size()));
        }

        result.modules.push_back(std::move(module));
    }

    result.ok = true;
    LOGF("[Firmware] INFO: Parsed %u files, %u PRX modules, %u other ELF files\n",
         static_cast<uint32_t>(result.entries.size()),
         prx_count, elf_count);

    return result;
}

std::string PupParser::GetFileName(uint64_t file_id) {
    for (const auto& known: kKnownFiles) {
        if (known.id == file_id) {
            return known.name;
        }
    }

    // Fallback: hex ID
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%08llX", static_cast<unsigned long long>(file_id));
    return std::string(buf);
}

bool PupParser::IsPrxModule(uint64_t file_id) {
    for (const auto& known: kKnownFiles) {
        if (known.id == file_id) {
            return known.is_prx;
        }
    }

    // PS5 PRX modules are typically in the 0x1000-0x7FFF range
    return (file_id >= 0x1000 && file_id <= 0x7FFF);
}

bool PupParser::ValidateHeader(const PupHeader& hdr, uint64_t file_size) {
    // Check magic number ("pup")
    if (hdr.magic != 0x70757000) {
        return false;
    }

    // Sanity checks
    if (hdr.file_size != file_size) {
        LOGF("[Firmware] WARN: Header file_size (%llu) != actual (%llu)\n",
             static_cast<unsigned long long>(hdr.file_size),
             static_cast<unsigned long long>(file_size));
        // Don't fail - some versions may have this mismatch
    }

    if (hdr.num_files == 0 || hdr.num_files > 10000) {
        return false;
    }

    return true;
}

bool PupParser::IsPrxData(const std::vector<uint8_t>& data) {
    // Check minimum size for ELF header
    if (data.size() < sizeof(PrxHeader)) {
        return false;
    }

    // Check ELF magic
    if (data[0] != 0x7F || data[1] != 'E' || data[2] != 'L' || data[3] != 'F') {
        return false;
    }

    // Check if it's a 64-bit ELF (PS5 is x86_64)
    if (data[4] != 2) { // EI_CLASS = ELFCLASS64
        return false;
    }

    // Parse the ELF header to check type
    const auto* elf_hdr = reinterpret_cast<const PrxHeader*>(data.data());

    // PS5 PRX modules use ET_SCE_DYNEXEC (0xFE00) or ET_SCE_DYNAMIC (0xFE10)
    // Accept a wider range to handle format variations
    const uint16_t type = elf_hdr->type;
    const bool is_sce_type = (type == 0xFE00 || type == 0xFE10 ||
                             type == 0xFE0C || type == 0xFE18 ||
                             type == 0xFEE0 || type == 0xFEC0);

    if (!is_sce_type) {
        LOGF("[Firmware] DEBUG: ELF type=0x%04X (not SCE PRX type)\n", type);
    }

    // Must be x86_64
    const bool is_x86_64 = (elf_hdr->machine == 0x3E);

    return is_sce_type && is_x86_64;
}

std::string PupParser::ExtractModuleName(const std::string& path, uint64_t file_id) {
    // If we have a path with a filename, use it
    if (!path.empty()) {
        const auto last_slash = path.find_last_of('/');
        if (last_slash != std::string::npos) {
            return path.substr(last_slash + 1);
        }
        return path;
    }

    // Otherwise, use the file_id lookup
    return GetFileName(file_id);
}

} // namespace Libs::Firmware
