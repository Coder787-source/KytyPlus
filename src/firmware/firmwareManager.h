#ifndef KYTY_FIRMWARE_FIRMWARE_MANAGER_H_
#define KYTY_FIRMWARE_FIRMWARE_MANAGER_H_

#include "firmware/pupParser.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Libs::Firmware {

// Firmware installation state and LLE/HLE management
class FirmwareManager {
public:
    // Get the singleton instance
    static FirmwareManager& Instance();

    // Install firmware from a .pup file
    // Returns true on success, error message in result.error if failed
    struct InstallResult {
        bool ok;
        std::string error;
        std::string version;
        uint32_t modules_installed;
    };
    InstallResult InstallFromPup(const std::string& pup_path);

    // Check if firmware is installed
    bool IsInstalled() const { return m_installed; }

    // Get firmware version string
    std::string GetVersion() const { return m_version; }

    // Get list of installed modules
    std::vector<FirmwareModule> GetModules() const;

    // Check if a specific module is available via LLE
    bool HasModule(const std::string& module_name) const;

    // Get the firmware directory path
    static std::filesystem::path GetFirmwareDir();

    // Check if a keys.bin is available alongside a given .pup path.
    // Returns true if the file exists and has the correct KPUP magic header.
    static bool HasKeysFile(const std::string& pup_path);

    // Initialize (called at emulator startup)
    void Initialize();

    // Shutdown
    void Shutdown();

private:
    FirmwareManager() = default;
    ~FirmwareManager() = default;

    // Extract all modules from parse result to firmware directory
    bool ExtractModules(const PupParseResult& result);

    // Scan firmware directory for installed modules
    void ScanInstalledModules();

    // Validate firmware integrity
    bool ValidateInstallation();

    mutable std::mutex m_mutex;
    bool m_installed = false;
    std::string m_version;
    std::map<std::string, FirmwareModule> m_modules;
};

} // namespace Libs::Firmware

#endif /* KYTY_FIRMWARE_FIRMWARE_MANAGER_H_ */
