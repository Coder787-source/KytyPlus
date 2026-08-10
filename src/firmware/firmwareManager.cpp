#include "firmware/firmwareManager.h"
#include "firmware/pupKeyStore.h"
#include "common/common.h"
#include "common/logging/log.h"

#include <cstring>
#include <fstream>

namespace Libs::Firmware {

FirmwareManager& FirmwareManager::Instance() {
    static FirmwareManager instance;
    return instance;
}

std::filesystem::path FirmwareManager::GetFirmwareDir() {
    // Firmware stored in: firmware/ps5/ relative to working directory
    return std::filesystem::path("firmware/ps5");
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

    // --- Try to load user-supplied keys from keys.bin in the same directory ---
    const auto key_store = PupKeyStore::LoadSiblingOf(pup_path);

    // Do a preliminary scan to detect whether the PUP has encrypted entries.
    // We call Parse() first (cheap, reads only the container structure) and
    // check the modules for encryption.  If any encrypted module is found and
    // there are no keys we bail out immediately with a clear message.
    auto probe = PupParser::Parse(pup_path);
    if (!probe.ok) {
        result.ok = false;
        result.error = "Failed to parse PUP file: " + probe.error;
        return result;
    }

    bool has_encrypted = false;
    for (const auto& module : probe.modules) {
        if (module.is_encrypted) {
            has_encrypted = true;
            break;
        }
    }

    if (has_encrypted && key_store.IsEmpty()) {
        result.ok = false;
        if (key_store.WasMalformed()) {
            result.error =
                "A keys.bin was found alongside the .pup but it is malformed (bad magic or version).\n"
                "Please verify your keys.bin is in the correct format.\n"
                "Expected location: " + key_store.KeysPath();
        } else if (std::filesystem::exists(key_store.KeysPath())) {
            // File exists and is well-formed but contains zero key entries
            result.error =
                "A keys.bin was found alongside the .pup but it contains no key entries.\n"
                "Ensure your keys.bin was correctly dumped from your PS5 console and is not empty.\n"
                "Expected location: " + key_store.KeysPath();
        } else {
            result.error =
                "This is an official encrypted .pup and no keys.bin was found alongside it.\n"
                "Place your own keys.bin (dumped from your PS5 console) in the same\n"
                "directory as the .pup file. The emulator never provides keys.\n"
                "Expected location: " + key_store.KeysPath();
        }
        LOGF("[Firmware] ERROR: %s\n", result.error.c_str());
        return result;
    }

    // --- Parse (and decrypt if needed) ---
    PupParseResult parse_result;
    if (has_encrypted) {
        LOGF("[Firmware] INFO: Encrypted PUP detected, decrypting with user-supplied keys.bin\n");
        parse_result = PupParser::ParseWithKeys(pup_path, key_store);
    } else {
        LOGF("[Firmware] INFO: PUP appears unencrypted (no keys required)\n");
        parse_result = std::move(probe); // reuse the probe result
    }

    if (!parse_result.ok) {
        result.ok = false;
        result.error = "Failed to parse PUP file: " + parse_result.error;
        return result;
    }

    // Create firmware directory
    const auto fw_dir = GetFirmwareDir();
    std::error_code ec;
    std::filesystem::create_directories(fw_dir, ec);
    if (ec) {
        result.ok = false;
        result.error = "Failed to create firmware directory: " + ec.message();
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

bool FirmwareManager::HasKeysFile(const std::string& pup_path) {
    const auto store = PupKeyStore::LoadSiblingOf(pup_path);
    // File must exist, be well-formed (valid magic/version), AND contain
    // at least one key entry.  A blank or zero-entry keys.bin is treated
    // identically to a missing file — the user will see the same "keys
    // required" dialog in the launcher.
    return !store.WasMalformed() && !store.IsEmpty();
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
        // Parse the ELF header to check type (use public interface)
        module.is_valid = module.is_prx;

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
