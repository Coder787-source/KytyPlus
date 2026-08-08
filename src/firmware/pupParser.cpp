#include "firmware/pupParser.h"
#include "common/logging/log.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Libs::Firmware {

// Known PS5 firmware file IDs (partial list from reverse engineering)
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
    PupHeader header {};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file.good()) {
        result.ok = false;
        result.error = "Failed to read PUP header";
        return result;
    }

    if (!ValidateHeader(header, file_size)) {
        result.ok = false;
        result.error = "Invalid PUP header (magic=" +
                       std::to_string(header.magic) + ")";
        return result;
    }

    result.header = header;

    LOGF("[Firmware] INFO: PUP version=%u, files=%u, flags=0x%08X\n",
         header.version, header.num_files, header.flags);

    // Read file entries
    const uint64_t entries_offset = sizeof(PupHeader);
    file.seekg(entries_offset, std::ios::beg);

    for (uint32_t i = 0; i < header.num_files; i++) {
        PupFileEntry entry {};
        file.read(reinterpret_cast<char*>(&entry), sizeof(entry));
        if (!file.good()) {
            result.ok = false;
            result.error = "Failed to read file entry " + std::to_string(i);
            return result;
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

        // Check if this is a PRX module
        const bool is_prx = IsPrxData(data);

        FirmwareModule module {};
        module.file_id = entry.file_id;
        module.data = std::move(data);
        module.is_prx = is_prx;
        module.is_valid = true;
        module.name = ExtractModuleName("", entry.file_id);
        module.path = ""; // Will be set during extraction

        result.modules.push_back(std::move(module));

        if (is_prx) {
            LOGF("[Firmware] INFO: Found PRX module: %s (0x%08llX, %llu bytes)\n",
                 module.name.c_str(),
                 static_cast<unsigned long long>(entry.file_id),
                 static_cast<unsigned long long>(entry.size));
        }
    }

    result.ok = true;
    LOGF("[Firmware] INFO: Parsed %u files, %u PRX modules\n",
         static_cast<uint32_t>(result.entries.size()),
         static_cast<uint32_t>(result.modules.size()));

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
    const bool is_sce_exec = (elf_hdr->type == 0xFE00 || elf_hdr->type == 0xFE10);

    // Must be x86_64
    const bool is_x86_64 = (elf_hdr->machine == 0x3E);

    return is_sce_exec && is_x86_64;
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
