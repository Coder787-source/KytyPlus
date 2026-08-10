#ifndef KYTY_FIRMWARE_PUP_KEY_STORE_H_
#define KYTY_FIRMWARE_PUP_KEY_STORE_H_

// PupKeyStore - user-supplied decryption keys for official Sony .pup files.
//
// Keys are loaded from a "keys.bin" file that MUST reside in the SAME directory
// as the .pup being installed. The emulator never ships, hardcodes, or generates
// any keys — users must dump them from their own hardware.
//
// Binary format of keys.bin:
//   [0..3]   Magic: 0x4B 0x50 0x55 0x50  ("KPUP")
//   [4..7]   Version: uint32_t LE (currently 1)
//   [8..11]  Entry count: uint32_t LE
//   [12..15] Reserved (must be 0)
//   Entries (40 bytes each):
//     [+0..+7]   entry_id : uint64_t LE   (matches PUP file-table entry ID)
//     [+8..+23]  key      : 16 raw bytes  (AES-128 key)
//     [+24..+39] iv       : 16 raw bytes  (AES-128 IV / nonce)
//
// If keys.bin is absent, malformed, or empty the store reports IsEmpty() == true
// and the emulator refuses to install the encrypted .pup.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libs::Firmware {

// One AES-128 key+IV pair for a single PUP entry.
struct PupSegmentKey {
    uint64_t                entry_id;  // Matches the PupFileEntry::file_id field
    std::array<uint8_t, 16> key;       // AES-128 key (16 bytes)
    std::array<uint8_t, 16> iv;        // AES-128 IV / initial counter (16 bytes)
};

// Magic bytes at offset 0 of a valid keys.bin
static constexpr uint8_t KEYS_BIN_MAGIC[4] = {0x4B, 0x50, 0x55, 0x50}; // "KPUP"
static constexpr uint32_t KEYS_BIN_VERSION  = 1u;
static constexpr size_t   KEYS_BIN_ENTRY_SIZE = 40u; // 8 + 16 + 16
static constexpr size_t   KEYS_BIN_HEADER_SIZE = 16u;

// Sanity cap: a legitimate firmware build only references a few hundred keys at
// most. Rejecting absurd entry_count values up front prevents a user-supplied
// keys.bin from triggering a multi-gigabyte allocation before the size check.
static constexpr uint32_t KEYS_BIN_MAX_ENTRIES = 4096u;

class PupKeyStore {
public:
    // Load keys.bin from the same directory as pup_path.
    // e.g. pup_path = "/games/PS5UPDATE.PUP" -> looks for "/games/keys.bin"
    // Returns an empty store (IsEmpty() == true) on any failure.
    static PupKeyStore LoadSiblingOf(const std::string& pup_path);

    // Returns true if no keys were loaded (file missing, malformed, or zero entries).
    bool IsEmpty() const { return m_keys.empty(); }

    // Returns the number of loaded key entries.
    size_t Count() const { return m_keys.size(); }

    // Returns true if a key is available for the given entry ID.
    bool HasKey(uint64_t entry_id) const;

    // Returns the key for the given entry ID, or std::nullopt if not found.
    std::optional<PupSegmentKey> GetKey(uint64_t entry_id) const;

    // The path to the keys.bin that was (or should be) loaded.
    // Useful for error messages even when loading failed.
    const std::string& KeysPath() const { return m_keys_path; }

    // True if the file existed but was malformed (vs. simply absent).
    bool WasMalformed() const { return m_was_malformed; }

private:
    std::unordered_map<uint64_t, PupSegmentKey> m_keys;
    std::string m_keys_path;
    bool m_was_malformed = false;
};

} // namespace Libs::Firmware

#endif /* KYTY_FIRMWARE_PUP_KEY_STORE_H_ */
