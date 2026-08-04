#include "saveDataChecksum.h"
#include "common/logging/log.h"
#include <cstring>
#include <algorithm>

namespace Kyty::Libs {

//=============================================================================
// SaveFileHeader Implementation
//=============================================================================

SaveFileHeader::SaveFileHeader() 
    : magic(0x4B595459) // "KYTY"
    , version(1)
    , dataSize(0)
    , checksumType(EChecksumType::CRC32)
    , timestamp(0)
{
    std::memset(checksum, 0, sizeof(checksum));
    std::memset(gameId, 0, sizeof(gameId));
    std::memset(saveName, 0, sizeof(saveName));
}

//=============================================================================
// SaveDataChecksum Implementation
//=============================================================================

SaveDataChecksum& SaveDataChecksum::Instance() {
    static SaveDataChecksum instance;
    return instance;
}

void SaveDataChecksum::Initialize() {
    if (m_initialized) {
        return;
    }
    
    LOGF("[SaveChecksum] Initializing checksum system...\n");
    
    InitializeCRC32Table();
    InitializeGTA3SaveChecksums();
    
    m_initialized = true;
    
    LOGF("[SaveChecksum] Checksum system initialized\n");
}

void SaveDataChecksum::Shutdown() {
    LOGF("[SaveChecksum] Shutting down checksum system...\n");
    m_initialized = false;
}

void SaveDataChecksum::InitializeCRC32Table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (uint32_t j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        m_crc32Table[i] = crc;
    }
}

uint32_t SaveDataChecksum::CalculateCRC32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < size; i++) {
        crc = m_crc32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

void SaveDataChecksum::CalculateMD5(const uint8_t* data, size_t size, uint8_t* outHash) {
    // Simplified MD5 implementation (stub)
    // In production, use a proper MD5 library
    std::memset(outHash, 0, 16);
    
    // Simple hash for demonstration
    uint32_t hash[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    
    for (size_t i = 0; i < size; i += 16) {
        size_t remaining = std::min(size - i, size_t(16));
        for (size_t j = 0; j < remaining; j++) {
            hash[j % 4] ^= data[i + j];
            hash[j % 4] = (hash[j % 4] << 5) | (hash[j % 4] >> 27);
        }
    }
    
    std::memcpy(outHash, hash, 16);
    
    LOGF("[SaveChecksum] Calculated MD5 hash\n");
}

void SaveDataChecksum::CalculateSHA1(const uint8_t* data, size_t size, uint8_t* outHash) {
    // Simplified SHA1 implementation (stub)
    // In production, use a proper SHA1 library
    std::memset(outHash, 0, 20);
    
    // Simple hash for demonstration
    uint32_t hash[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    
    for (size_t i = 0; i < size; i += 20) {
        size_t remaining = std::min(size - i, size_t(20));
        for (size_t j = 0; j < remaining; j++) {
            hash[j % 5] ^= data[i + j];
            hash[j % 5] = (hash[j % 5] << 5) | (hash[j % 5] >> 27);
        }
    }
    
    std::memcpy(outHash, hash, 20);
    
    LOGF("[SaveChecksum] Calculated SHA1 hash\n");
}

bool SaveDataChecksum::CalculateChecksum(const uint8_t* data, size_t size, EChecksumType type,
                                          uint8_t* outChecksum, size_t checksumSize) {
    if (!data || !outChecksum || checksumSize == 0) {
        return false;
    }
    
    switch (type) {
        case EChecksumType::CRC32:
            if (checksumSize < 4) return false;
            *reinterpret_cast<uint32_t*>(outChecksum) = CalculateCRC32(data, size);
            return true;
            
        case EChecksumType::MD5:
            if (checksumSize < 16) return false;
            CalculateMD5(data, size, outChecksum);
            return true;
            
        case EChecksumType::SHA1:
            if (checksumSize < 20) return false;
            CalculateSHA1(data, size, outChecksum);
            return true;
            
        case EChecksumType::Custom: {
            // Custom checksum - XOR based
            if (checksumSize < 4) return false;
            uint32_t custom = 0;
            for (size_t i = 0; i < size; i++) {
                custom ^= (data[i] << ((i % 4) * 8));
            }
            *reinterpret_cast<uint32_t*>(outChecksum) = custom;
            return true;
        }
            
        default:
            return false;
    }
}

bool SaveDataChecksum::ValidateChecksum(const uint8_t* data, size_t size,
                                         const uint8_t* expectedChecksum, size_t checksumSize,
                                         EChecksumType type) {
    if (!m_validationEnabled) {
        return true; // Skip validation if disabled
    }
    
    if (!data || !expectedChecksum || checksumSize == 0) {
        return false;
    }
    
    uint8_t calculatedChecksum[32] = {0};
    
    if (!CalculateChecksum(data, size, type, calculatedChecksum, checksumSize)) {
        return false;
    }
    
    bool valid = (std::memcmp(calculatedChecksum, expectedChecksum, checksumSize) == 0);
    
    if (!valid) {
        LOGF("[SaveChecksum] Checksum validation failed!\n");
    } else {
        LOGF("[SaveChecksum] Checksum validation passed\n");
    }
    
    return valid;
}

bool SaveDataChecksum::AddChecksumToHeader(SaveFileHeader& header, const uint8_t* data, size_t dataSize) {
    if (!data || dataSize == 0) {
        return false;
    }
    
    header.dataSize = static_cast<uint32_t>(dataSize);
    
    size_t checksumSize = GetChecksumSize(header.checksumType);
    if (checksumSize > sizeof(header.checksum)) {
        checksumSize = sizeof(header.checksum);
    }
    
    return CalculateChecksum(data, dataSize, header.checksumType,
                             header.checksum, checksumSize);
}

bool SaveDataChecksum::VerifyHeaderChecksum(const SaveFileHeader& header, const uint8_t* data, size_t dataSize) {
    if (!m_validationEnabled) {
        return true;
    }
    
    if (!data || dataSize == 0) {
        return false;
    }
    
    size_t checksumSize = GetChecksumSize(header.checksumType);
    if (checksumSize > sizeof(header.checksum)) {
        checksumSize = sizeof(header.checksum);
    }
    
    return ValidateChecksum(data, dataSize, header.checksum, checksumSize, header.checksumType);
}

SaveFileHeader SaveDataChecksum::CreateHeader(const std::string& gameId, const std::string& saveName,
                                               uint32_t version, EChecksumType checksumType) {
    SaveFileHeader header;
    header.version = version;
    header.checksumType = checksumType;
    header.timestamp = 0; // Would use real timestamp in production
    
    std::strncpy(header.gameId, gameId.c_str(), sizeof(header.gameId) - 1);
    std::strncpy(header.saveName, saveName.c_str(), sizeof(header.saveName) - 1);
    
    return header;
}

size_t SaveDataChecksum::GetChecksumSize(EChecksumType type) const {
    switch (type) {
        case EChecksumType::CRC32: return 4;
        case EChecksumType::MD5: return 16;
        case EChecksumType::SHA1: return 20;
        case EChecksumType::Custom: return 4;
        default: return 0;
    }
}

void SaveDataChecksum::SetValidationEnabled(bool enabled) {
    if (m_validationEnabled != enabled) {
        m_validationEnabled = enabled;
        LOGF("[SaveChecksum] Validation %s\n", enabled ? "enabled" : "disabled");
    }
}

bool SaveDataChecksum::IsValidationEnabled() const {
    return m_validationEnabled;
}

//=============================================================================
// GTA 3 DE Save Checksum Support
//=============================================================================

void InitializeGTA3SaveChecksums() {
    LOGF("[SaveChecksum] Initializing GTA 3 DE save checksum support...\n");
    
    // GTA 3 DE uses a custom checksum format
    // This is handled by the Custom checksum type
}

bool ValidateGTA3Save(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(SaveFileHeader)) {
        return false;
    }
    
    const SaveFileHeader* header = reinterpret_cast<const SaveFileHeader*>(data);
    
    // Check magic number
    if (header->magic != 0x4B595459) {
        LOGF("[SaveChecksum] Invalid GTA 3 save magic number\n");
        return false;
    }
    
    // Validate checksum
    const uint8_t* saveData = data + sizeof(SaveFileHeader);
    size_t saveDataSize = size - sizeof(SaveFileHeader);
    
    auto& checksum = SaveDataChecksum::Instance();
    return checksum.VerifyHeaderChecksum(*header, saveData, saveDataSize);
}

bool RepairGTA3Save(uint8_t* data, size_t size) {
    if (!data || size < sizeof(SaveFileHeader)) {
        return false;
    }
    
    SaveFileHeader* header = reinterpret_cast<SaveFileHeader*>(data);
    
    // Recalculate checksum
    uint8_t* saveData = data + sizeof(SaveFileHeader);
    size_t saveDataSize = size - sizeof(SaveFileHeader);
    
    auto& checksum = SaveDataChecksum::Instance();
    if (checksum.AddChecksumToHeader(*header, saveData, saveDataSize)) {
        LOGF("[SaveChecksum] Repaired GTA 3 save checksum\n");
        return true;
    }
    
    LOGF("[SaveChecksum] Failed to repair GTA 3 save\n");
    return false;
}

} // namespace Kyty::Libs
