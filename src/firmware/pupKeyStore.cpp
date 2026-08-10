#include "firmware/pupKeyStore.h"
#include "common/logging/log.h"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace Libs::Firmware {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Returns the directory containing pup_path joined with "keys.bin".
static std::string BuildKeysPath(const std::string& pup_path) {
    const auto parent = std::filesystem::path(pup_path).parent_path();
    return (parent / "keys.bin").string();
}

// ---------------------------------------------------------------------------
// PupKeyStore::LoadSiblingOf
// ---------------------------------------------------------------------------

PupKeyStore PupKeyStore::LoadSiblingOf(const std::string& pup_path) {
    PupKeyStore store;
    store.m_keys_path = BuildKeysPath(pup_path);

    // --- Open the file ---
    std::ifstream file(store.m_keys_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOGF("[Firmware] INFO: No keys.bin found alongside .pup (expected: %s)\n",
             store.m_keys_path.c_str());
        // Not malformed — simply absent.
        return store;
    }

    const auto file_size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    LOGF("[Firmware] INFO: Found keys.bin at: %s (%zu bytes)\n",
         store.m_keys_path.c_str(), file_size);

    // --- Read entire file into memory ---
    std::vector<uint8_t> data(file_size);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(file_size));
    if (!file.good()) {
        LOGF("[Firmware] ERROR: Failed to read keys.bin\n");
        store.m_was_malformed = true;
        return store;
    }

    // --- Validate header ---
    if (file_size < KEYS_BIN_HEADER_SIZE) {
        LOGF("[Firmware] ERROR: keys.bin is too small to contain a valid header (%zu bytes)\n",
             file_size);
        store.m_was_malformed = true;
        return store;
    }

    // Check magic
    if (std::memcmp(data.data(), KEYS_BIN_MAGIC, 4) != 0) {
        LOGF("[Firmware] ERROR: keys.bin has wrong magic (got 0x%02X%02X%02X%02X, "
             "expected 0x4B505550)\n",
             data[0], data[1], data[2], data[3]);
        store.m_was_malformed = true;
        return store;
    }

    // Check version
    uint32_t version = 0;
    std::memcpy(&version, data.data() + 4, sizeof(version));
    if (version != KEYS_BIN_VERSION) {
        LOGF("[Firmware] ERROR: keys.bin version %u is unsupported (expected %u)\n",
             version, KEYS_BIN_VERSION);
        store.m_was_malformed = true;
        return store;
    }

    // Read entry count
    uint32_t entry_count = 0;
    std::memcpy(&entry_count, data.data() + 8, sizeof(entry_count));

    // Reject absurd entry counts before doing any size math or allocation.
    // Without this guard, entry_count = 0xFFFFFFFF would skip the truncation
    // check (file_size < expected_size always true) and trigger a ~172GB
    // reserve() on user-supplied input.
    if (entry_count > KEYS_BIN_MAX_ENTRIES) {
        LOGF("[Firmware] ERROR: keys.bin claims %u entries (max %u) — refusing to allocate\n",
             entry_count, KEYS_BIN_MAX_ENTRIES);
        store.m_was_malformed = true;
        return store;
    }

    // Validate total size
    const size_t expected_size = KEYS_BIN_HEADER_SIZE + static_cast<size_t>(entry_count) * KEYS_BIN_ENTRY_SIZE;
    if (file_size < expected_size) {
        LOGF("[Firmware] ERROR: keys.bin claims %u entries but file is too small "
             "(expected %zu bytes, got %zu)\n",
             entry_count, expected_size, file_size);
        store.m_was_malformed = true;
        return store;
    }

    if (entry_count == 0) {
        LOGF("[Firmware] WARN: keys.bin contains zero entries — treating as empty\n");
        return store; // IsEmpty() will be true; not malformed
    }

    // --- Parse entries ---
    // Each entry layout (40 bytes, all fields little-endian):
    //   [0..7]   entry_id (uint64_t)
    //   [8..23]  key      (16 raw bytes)
    //   [24..39] iv       (16 raw bytes)

    store.m_keys.reserve(entry_count);
    const uint8_t* cursor = data.data() + KEYS_BIN_HEADER_SIZE;

    for (uint32_t i = 0; i < entry_count; ++i) {
        PupSegmentKey seg {};

        std::memcpy(&seg.entry_id, cursor,      sizeof(uint64_t));
        std::memcpy(seg.key.data(),  cursor + 8, 16);
        std::memcpy(seg.iv.data(),   cursor + 24, 16);

        LOGF("[Firmware] DEBUG: Loaded key for entry 0x%016llX\n",
             static_cast<unsigned long long>(seg.entry_id));

        store.m_keys[seg.entry_id] = seg;
        cursor += KEYS_BIN_ENTRY_SIZE;
    }

    LOGF("[Firmware] INFO: Loaded %u key(s) from keys.bin\n", entry_count);
    return store;
}

// ---------------------------------------------------------------------------
// PupKeyStore::HasKey / GetKey
// ---------------------------------------------------------------------------

bool PupKeyStore::HasKey(uint64_t entry_id) const {
    return m_keys.find(entry_id) != m_keys.end();
}

std::optional<PupSegmentKey> PupKeyStore::GetKey(uint64_t entry_id) const {
    const auto it = m_keys.find(entry_id);
    if (it == m_keys.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace Libs::Firmware
