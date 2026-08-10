#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace Kyty::Libs {

/**
 * Save Data Checksum Validation Module
 * 
 * This module provides checksum calculation and validation for save files
 * to prevent save corruption and detect tampering.
 * 
 * Supports: CRC32, MD5, SHA1, and custom checksum algorithms.
 */

// Checksum types
enum class EChecksumType : uint8_t {
    None = 0,
    CRC32 = 1,
    MD5 = 2,
    SHA1 = 3,
    Custom = 4
};

/**
 * Save file header with checksum
 */
struct SaveFileHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t dataSize;
    EChecksumType checksumType;
    uint8_t checksum[32]; // Supports up to SHA256
    uint64_t timestamp;
    char gameId[64];
    char saveName[128];
    
    SaveFileHeader();
};

/**
 * Save Data Checksum Manager
 */
class SaveDataChecksum {
public:
    static SaveDataChecksum& Instance();
    
    /**
     * Initialize checksum system
     */
    void Initialize();
    
    /**
     * Shutdown checksum system
     */
    void Shutdown();
    
    /**
     * Calculate CRC32 checksum
     * @param data Data buffer
     * @param size Data size
     * @return CRC32 checksum
     */
    uint32_t CalculateCRC32(const uint8_t* data, size_t size);
    
    /**
     * Calculate MD5 hash
     * @param data Data buffer
     * @param size Data size
     * @param outHash Output hash (16 bytes)
     */
    void CalculateMD5(const uint8_t* data, size_t size, uint8_t* outHash);
    
    /**
     * Calculate SHA1 hash
     * @param data Data buffer
     * @param size Data size
     * @param outHash Output hash (20 bytes)
     */
    void CalculateSHA1(const uint8_t* data, size_t size, uint8_t* outHash);
    
    /**
     * Calculate checksum for save data
     * @param data Save data buffer
     * @param size Data size
     * @param type Checksum type to use
     * @param outChecksum Output checksum buffer
     * @param checksumSize Size of checksum buffer
     * @return true if calculation succeeded
     */
    bool CalculateChecksum(const uint8_t* data, size_t size, EChecksumType type,
                           uint8_t* outChecksum, size_t checksumSize);
    
    /**
     * Validate save file checksum
     * @param data Save data buffer
     * @param size Data size
     * @param expectedChecksum Expected checksum
     * @param checksumSize Size of checksum
     * @param type Checksum type
     * @return true if checksum matches
     */
    bool ValidateChecksum(const uint8_t* data, size_t size,
                          const uint8_t* expectedChecksum, size_t checksumSize,
                          EChecksumType type);
    
    /**
     * Add checksum to save file header
     * @param header Save file header
     * @param data Save data
     * @param dataSize Data size
     * @return true if checksum was added
     */
    bool AddChecksumToHeader(SaveFileHeader& header, const uint8_t* data, size_t dataSize);
    
    /**
     * Verify save file header checksum
     * @param header Save file header
     * @param data Save data
     * @param dataSize Data size
     * @return true if checksum is valid
     */
    bool VerifyHeaderChecksum(const SaveFileHeader& header, const uint8_t* data, size_t dataSize);
    
    /**
     * Create a new save file header
     * @param gameId Game identifier
     * @param saveName Save name
     * @param version Save version
     * @param checksumType Checksum type to use
     * @return New save file header
     */
    SaveFileHeader CreateHeader(const std::string& gameId, const std::string& saveName,
                                uint32_t version, EChecksumType checksumType);
    
    /**
     * Get the size of a checksum for a given type
     * @param type Checksum type
     * @return Checksum size in bytes
     */
    size_t GetChecksumSize(EChecksumType type) const;
    
    /**
     * Enable/disable checksum validation
     * @param enabled Enable/disable
     */
    void SetValidationEnabled(bool enabled);
    
    /**
     * Check if validation is enabled
     * @return true if validation is enabled
     */
    bool IsValidationEnabled() const;
    
private:
    SaveDataChecksum() = default;
    ~SaveDataChecksum() = default;
    
    SaveDataChecksum(const SaveDataChecksum&) = delete;
    SaveDataChecksum& operator=(const SaveDataChecksum&) = delete;
    
    uint32_t m_crc32Table[256];
    bool m_initialized = false;
    bool m_validationEnabled = true;
    
    void InitializeCRC32Table();
};

/**
 * Initialize GTA 3 DE save checksum support
 */
void InitializeGTA3SaveChecksums();

/**
 * Validate GTA 3 DE save file
 * @param data Save data
 * @param size Data size
 * @return true if save is valid
 */
bool ValidateGTA3Save(const uint8_t* data, size_t size);

/**
 * Repair corrupted GTA 3 DE save file
 * @param data Save data
 * @param size Data size
 * @return true if repair succeeded
 */
bool RepairGTA3Save(uint8_t* data, size_t size);

} // namespace Kyty::Libs
