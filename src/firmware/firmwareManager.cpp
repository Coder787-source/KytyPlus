#include "firmware/firmwareManager.h"
#include "common/common.h"
#include "common/logging/log.h"

#include <fstream>

namespace Libs::Firmware {

FirmwareManager& FirmwareManager::Instance() {
    static FirmwareManager instance;
    return instance;
}

std::filesystem::path FirmwareManager::GetFirmwareDir() {
    // Firmware stored in: <user_data_dir>/firmware/ps5/
    const auto base_dir = Common::GetUserDirPath();
    return base_dir / "firmware" / "ps5";
}

void FirmwareManager::Initialize() {
    std::lock_guard lock(m_mutex);

    const auto fw_dir = GetFirmwareDir();
    if (!std::filesystem::exists(fw_dir)) {
        LOGF("[Firmware] INFO: No firmware installed\n");
        m_installed = false;
        return;
    }

    // Scan for installed modules
    ScanInstalledModules();

    if (!m_modules.empty()) {
        m_installed = true;
        LOGF("[Firmware] INFO: Loaded %u firmware modules\n",
             static_cast<uint32_t>(m_modules.size()));
    }
}

void FirmwareManager::Shutdown() {
    std::lock_guard lock(m_mutex);
    m_modules.clear();
    m_installed = false;
}

FirmwareManager::InstallResult FirmwareManager::InstallFromPup(const std::string& pup_path) {
    InstallResult result {};

    LOGF("[Firmware] INFO: Installing firmware from: %s\n", pup_path.c_str());

    // Parse the PUP file
    auto parse_result = PupParser::Parse(pup_path);
    if (!parse_result.ok) {
        result.ok = false;
        result.error = "Failed to parse PUP file: " + parse_result.error;
        return result;
    }

    // Create firmware directory
    const auto fw_dir = GetFirmwareDir();
    try {
        std::filesystem::create_directories(fw_dir);
    } catch (const std::exception& e) {
        result.ok = false;
        result.error = "Failed to create firmware directory: " + std::string(e.what());
        return result;
    }

    // Extract modules
    if (!ExtractModules(parse_result)) {
        result.ok = false;
        result.error = "Failed to extract firmware modules";
        return result;
    }

    // Validate installation
    if (!ValidateInstallation()) {
        result.ok = false;
        result.error = "Firmware validation failed";
        return result;
    }

    // Update state
    std::lock_guard lock(m_mutex);
    m_installed = true;
    m_modules.clear();
    ScanInstalledModules();

    result.ok = true;
    result.version = "Unknown"; // Would extract from version.txt
    result.modules_installed = static_cast<uint32_t>(m_modules.size());

    LOGF("[Firmware] INFO: Firmware installed successfully (%u modules)\n",
         result.modules_installed);

    return result;
}

std::vector<FirmwareModule> FirmwareManager::GetModules() const {
    std::lock_guard lock(m_mutex);
    std::vector<FirmwareModule> modules;
    modules.reserve(m_modules.size());
    for (const auto& [name, module]: m_modules) {
        modules.push_back(module);
    }
    return modules;
}

bool FirmwareManager::HasModule(const std::string& module_name) const {
    std::lock_guard lock(m_mutex);
    return m_modules.find(module_name) != m_modules.end();
}

bool FirmwareManager::ExtractModules(const PupParseResult& result) {
    const auto fw_dir = GetFirmwareDir();
    uint32_t extracted = 0;
    uint32_t failed = 0;

    for (const auto& module: result.modules) {
        if (!module.is_prx && !module.is_valid) {
            continue; // Skip invalid files
        }

        // Write module to disk
        const auto module_path = fw_dir / module.name;
        std::ofstream out(module_path, std::ios::binary);
        if (!out.is_open()) {
            LOGF("[Firmware] ERROR: Failed to write module: %s\n",
                 module.name.c_str());
            failed++;
            continue;
        }

        out.write(reinterpret_cast<const char*>(module.data.data()),
                  static_cast<std::streamsize>(module.data.size()));
        out.close();

        // Verify the file was written correctly
        if (!std::filesystem::exists(module_path)) {
            LOGF("[Firmware] ERROR: Module file not created: %s\n", module.name.c_str());
            failed++;
            continue;
        }

        const auto actual_size = std::filesystem::file_size(module_path);
        if (actual_size != module.data.size()) {
            LOGF("[Firmware] ERROR: Module size mismatch: %s (expected %llu, got %llu)\n",
                 module.name.c_str(),
                 static_cast<unsigned long long>(module.data.size()),
                 static_cast<unsigned long long>(actual_size));
            failed++;
            continue;
        }

        extracted++;
        LOGF("[Firmware] INFO: Extracted %s (%llu bytes)\n",
             module.name.c_str(),
             static_cast<unsigned long long>(module.data.size()));
    }

    // Write version file if available
    for (const auto& module: result.modules) {
        if (module.name == "version.txt" || module.file_id == 0x300) {
            const auto version_path = fw_dir / "version.txt";
            std::ofstream out(version_path, std::ios::binary);
            if (out.is_open()) {
                out.write(reinterpret_cast<const char*>(module.data.data()),
                          static_cast<std::streamsize>(module.data.size()));
                out.close();
                LOGF("[Firmware] INFO: Extracted version.txt\n");
            }
            break;
        }
    }

    LOGF("[Firmware] INFO: Extraction complete: %u succeeded, %u failed\n",
         extracted, failed);

    return extracted > 0;
}

void FirmwareManager::ScanInstalledModules() {
    const auto fw_dir = GetFirmwareDir();
    if (!std::filesystem::exists(fw_dir)) {
        return;
    }

    // Read version.txt if available
    const auto version_path = fw_dir / "version.txt";
    if (std::filesystem::exists(version_path)) {
        std::ifstream file(version_path);
        if (file.is_open()) {
            std::getline(file, m_version);
            LOGF("[Firmware] INFO: Firmware version: %s\n", m_version.c_str());
        }
    }

    // Scan for .prx files
    for (const auto& entry: std::filesystem::directory_iterator(fw_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto path = entry.path();
        const auto ext = path.extension().string();
        if (ext != ".prx" && ext != ".sprx") {
            continue;
        }

        // Read the file
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            continue;
        }

        const auto size = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));

        FirmwareModule module {};
        module.name = path.filename().string();
        module.path = path.string();
        module.data = std::move(data);
        module.is_prx = true;
        module.is_valid = PupParser::IsPrxData(module.data);

        if (module.is_valid) {
            m_modules[module.name] = std::move(module);
        } else {
            LOGF("[Firmware] WARN: Invalid PRX module: %s\n", module.name.c_str());
        }
    }
}

bool FirmwareManager::ValidateInstallation() {
    const auto fw_dir = GetFirmwareDir();

    // Must have at least kernel.prx or libkernel.prx
    const bool has_kernel =
        std::filesystem::exists(fw_dir / "kernel.prx") ||
        std::filesystem::exists(fw_dir / "libkernel.prx");

    if (!has_kernel) {
        LOGF("[Firmware] ERROR: Missing kernel module\n");
        return false;
    }

    return true;
}

} // namespace Libs::Firmware
