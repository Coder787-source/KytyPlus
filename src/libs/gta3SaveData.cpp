#include "libs/gta3SaveData.h"
#include "common/logging/log.h"
#include "common/file.h"
#include <fstream>
#include <cstring>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace Kyty::Libs {

// Global save manager
static GTA3SaveManager g_gta3SaveManager;

GTA3SaveManager& GetGTA3SaveManager() {
    return g_gta3SaveManager;
}

GTA3SaveManager::GTA3SaveManager() {
    m_slotExists.fill(false);
}

GTA3SaveManager::~GTA3SaveManager() {
    Shutdown();
}

bool GTA3SaveManager::Initialize(const std::string& saveDirectory) {
    if (m_initialized) {
        LOGF("[GTA3Save] WARNING: " "Already initialized");
        return true;
    }

    m_saveDirectory = saveDirectory;
    
    if (!EnsureDirectoryExists()) {
        LOGF("[GTA3Save] ERROR: " "Failed to create save directory: %s", m_saveDirectory.c_str());
        return false;
    }

    // Scan for existing saves
    for (int32_t i = 0; i < GTA3_MAX_SAVE_SLOTS; i++) {
        std::string filePath = GetSaveFilePath(i);
        m_slotExists[i] = fs::exists(filePath);
    }

    m_initialized = true;
    
    LOGF("[GTA3Save] INFO: " "Initialized: %s", m_saveDirectory.c_str());
    LOGF("[GTA3Save] INFO: " "Found %d existing saves", GetSaveCount());

    return true;
}

void GTA3SaveManager::Shutdown() {
    m_initialized = false;
    m_slotExists.fill(false);
    LOGF("[GTA3Save] INFO: " "Shutdown complete");
}

bool GTA3SaveManager::EnsureDirectoryExists() const {
    try {
        if (!fs::exists(m_saveDirectory)) {
            fs::create_directories(m_saveDirectory);
        }
        return true;
    } catch (const std::exception& e) {
        LOGF("[GTA3Save] ERROR: " "Failed to create directory: %s", e.what());
        return false;
    }
}

std::string GTA3SaveManager::GetSaveFilePath(int32_t slotId) const {
    if (slotId < 0 || slotId >= GTA3_MAX_SAVE_SLOTS) {
        return "";
    }
    
    char filename[64];
    snprintf(filename, sizeof(filename), "gta3de_save_%02d.bin", slotId);
    
    return m_saveDirectory + "/" + filename;
}

bool GTA3SaveManager::SaveGame(int32_t slotId, const GTA3SaveData& data) {
    if (!m_initialized) {
        LOGF("[GTA3Save] ERROR: " "Not initialized");
        return false;
    }
    
    if (slotId < 0 || slotId >= GTA3_MAX_SAVE_SLOTS) {
        LOGF("[GTA3Save] ERROR: " "Invalid slot ID: %d", slotId);
        return false;
    }

    // Create a copy to modify
    GTA3SaveData saveData = data;
    
    // Update header
    saveData.header.slotId = slotId;
    saveData.header.lastModified = static_cast<uint32_t>(
        std::chrono::system_clock::now().time_since_epoch().count() / 1000000000);
    
    // Calculate checksum
    saveData.header.checksum = CalculateChecksum(saveData);
    
    // Write to file
    std::string filePath = GetSaveFilePath(slotId);
    if (filePath.empty()) {
        LOGF("[GTA3Save] ERROR: " "Invalid file path for slot %d", slotId);
        return false;
    }
    
    try {
        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile) {
            LOGF("[GTA3Save] ERROR: " "Failed to open file for writing: %s", filePath.c_str());
            return false;
        }
        
        outFile.write(reinterpret_cast<const char*>(&saveData), sizeof(GTA3SaveData));
        outFile.close();
        
        if (!outFile) {
            LOGF("[GTA3Save] ERROR: " "Failed to write save data");
            return false;
        }
        
        m_slotExists[slotId] = true;
        
        LOGF("[GTA3Save] INFO: " "Saved game to slot %d: %s", slotId, saveData.header.saveName);
        LOGF("[GTA3Save] DEBUG: " "  Location: %s", saveData.header.locationName);
        LOGF("[GTA3Save] DEBUG: " "  Play time: %u seconds", saveData.header.playTimeSeconds);
        LOGF("[GTA3Save] DEBUG: " "  Mission: %u", saveData.header.missionId);
        
        return true;
        
    } catch (const std::exception& e) {
        LOGF("[GTA3Save] ERROR: " "Exception while saving: %s", e.what());
        return false;
    }
}

bool GTA3SaveManager::LoadGame(int32_t slotId, GTA3SaveData& data) {
    if (!m_initialized) {
        LOGF("[GTA3Save] ERROR: " "Not initialized");
        return false;
    }
    
    if (slotId < 0 || slotId >= GTA3_MAX_SAVE_SLOTS) {
        LOGF("[GTA3Save] ERROR: " "Invalid slot ID: %d", slotId);
        return false;
    }
    
    std::string filePath = GetSaveFilePath(slotId);
    if (!fs::exists(filePath)) {
        LOGF("[GTA3Save] DEBUG: " "No save found in slot %d", slotId);
        return false;
    }
    
    try {
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile) {
            LOGF("[GTA3Save] ERROR: " "Failed to open file for reading: %s", filePath.c_str());
            return false;
        }
        
        inFile.read(reinterpret_cast<char*>(&data), sizeof(GTA3SaveData));
        inFile.close();
        
        if (!inFile) {
            LOGF("[GTA3Save] ERROR: " "Failed to read save data");
            return false;
        }
        
        // Validate magic number
        if (data.header.magic != 0x47544133) { // "GTA3"
            LOGF("[GTA3Save] ERROR: " "Invalid save file magic: 0x%08X", data.header.magic);
            return false;
        }
        
        // Validate version
        if (data.header.version != GTA3_SAVE_VERSION) {
            LOGF("[GTA3Save] WARNING: " "Save version mismatch: expected %d, got %d",
                        GTA3_SAVE_VERSION, data.header.version);
            // Continue anyway for backwards compatibility
        }
        
        // Validate checksum
        uint32_t expectedChecksum = data.header.checksum;
        uint32_t actualChecksum = CalculateChecksum(data);
        
        if (expectedChecksum != actualChecksum) {
            LOGF("[GTA3Save] WARNING: " "Checksum mismatch: expected 0x%08X, got 0x%08X",
                        expectedChecksum, actualChecksum);
            // Continue anyway - some saves may have been modified
        }
        
        // Validate slot ID
        if (data.header.slotId != static_cast<uint32_t>(slotId)) {
            LOGF("[GTA3Save] WARNING: " "Slot ID mismatch: file says %d, expected %d",
                        data.header.slotId, slotId);
        }
        
        LOGF("[GTA3Save] INFO: " "Loaded game from slot %d: %s", slotId, data.header.saveName);
        LOGF("[GTA3Save] DEBUG: " "  Location: %s", data.header.locationName);
        LOGF("[GTA3Save] DEBUG: " "  Play time: %u seconds", data.header.playTimeSeconds);
        LOGF("[GTA3Save] DEBUG: " "  Mission: %u", data.header.missionId);
        LOGF("[GTA3Save] DEBUG: " "  Player health: %u", data.player.health);
        LOGF("[GTA3Save] DEBUG: " "  Player money: %u", data.player.money);
        
        return true;
        
    } catch (const std::exception& e) {
        LOGF("[GTA3Save] ERROR: " "Exception while loading: %s", e.what());
        return false;
    }
}

bool GTA3SaveManager::DeleteGame(int32_t slotId) {
    if (!m_initialized) {
        LOGF("[GTA3Save] ERROR: " "Not initialized");
        return false;
    }
    
    if (slotId < 0 || slotId >= GTA3_MAX_SAVE_SLOTS) {
        LOGF("[GTA3Save] ERROR: " "Invalid slot ID: %d", slotId);
        return false;
    }
    
    std::string filePath = GetSaveFilePath(slotId);
    
    try {
        if (fs::exists(filePath)) {
            fs::remove(filePath);
            m_slotExists[slotId] = false;
            LOGF("[GTA3Save] INFO: " "Deleted save from slot %d", slotId);
            return true;
        } else {
            LOGF("[GTA3Save] DEBUG: " "No save to delete in slot %d", slotId);
            return false;
        }
    } catch (const std::exception& e) {
        LOGF("[GTA3Save] ERROR: " "Exception while deleting: %s", e.what());
        return false;
    }
}

bool GTA3SaveManager::GetSaveInfo(int32_t slotId, GTA3SaveHeader& header) {
    if (!m_initialized) {
        return false;
    }
    
    if (slotId < 0 || slotId >= GTA3_MAX_SAVE_SLOTS) {
        return false;
    }
    
    std::string filePath = GetSaveFilePath(slotId);
    if (!fs::exists(filePath)) {
        return false;
    }
    
    try {
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile) {
            return false;
        }
        
        // Read only the header
        inFile.read(reinterpret_cast<char*>(&header), sizeof(GTA3SaveHeader));
        inFile.close();
        
        return inFile.gcount() == sizeof(GTA3SaveHeader);
        
    } catch (...) {
        return false;
    }
}

bool GTA3SaveManager::HasSave(int32_t slotId) const {
    if (slotId < 0 || slotId >= GTA3_MAX_SAVE_SLOTS) {
        return false;
    }
    
    return m_slotExists[slotId];
}

std::vector<int32_t> GTA3SaveManager::GetSaveSlots() const {
    std::vector<int32_t> slots;
    
    for (int32_t i = 0; i < GTA3_MAX_SAVE_SLOTS; i++) {
        if (m_slotExists[i]) {
            slots.push_back(i);
        }
    }
    
    return slots;
}

int32_t GTA3SaveManager::GetFirstEmptySlot() const {
    for (int32_t i = 0; i < GTA3_MAX_SAVE_SLOTS; i++) {
        if (!m_slotExists[i]) {
            return i;
        }
    }
    return -1; // No empty slots
}

uint32_t GTA3SaveManager::CalculateChecksum(const GTA3SaveData& data) {
    // Simple CRC32-like checksum
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
    size_t size = sizeof(GTA3SaveData);
    
    uint32_t checksum = 0xFFFFFFFF;
    
    for (size_t i = 0; i < size; i++) {
        checksum ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            checksum = (checksum >> 1) ^ ((checksum & 1) ? 0xEDB88320 : 0);
        }
    }
    
    return ~checksum;
}

bool GTA3SaveManager::ValidateChecksum(const GTA3SaveData& data) {
    return CalculateChecksum(data) == data.header.checksum;
}

bool GTA3SaveManager::ExportToJson(int32_t slotId, const std::string& jsonPath) {
    // Stub - would require JSON library
    LOGF("[GTA3Save] INFO: " "ExportToJson: slot=%d path=%s (stub)", slotId, jsonPath.c_str());
    return false;
}

bool GTA3SaveManager::ImportFromJson(const std::string& jsonPath, int32_t slotId) {
    // Stub - would require JSON library
    LOGF("[GTA3Save] INFO: " "ImportFromJson: path=%s slot=%d (stub)", jsonPath.c_str(), slotId);
    return false;
}

int32_t GTA3SaveManager::GetSaveCount() const {
    return static_cast<int32_t>(std::count(m_slotExists.begin(), m_slotExists.end(), true));
}

size_t GTA3SaveManager::GetTotalSaveSize() const {
    return GetSaveCount() * GTA3_SAVE_SIZE;
}

void GTA3SaveManager::CompressSave(GTA3SaveData& data) {
    // Stub - could implement compression if needed
    // For now, saves are uncompressed
}

void GTA3SaveManager::DecompressSave(GTA3SaveData& data) {
    // Stub - could implement decompression if needed
}

} // namespace Kyty::Libs
