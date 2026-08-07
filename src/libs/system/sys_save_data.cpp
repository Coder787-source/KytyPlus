#include "sys_save_data.h"
#include "logging.h"
#include "kernel/fs.h"
#include "crypto/aes.h"
#include <vector>

int sceSaveDataMount(int userId, const char* dirName, const char* mountPoint) {
    LOG_INFO("GTA V: SaveDataMount called (userId=%d, dir=%s)", userId, dirName);

    if (!dirName || !mountPoint) {
        return -1;
    }

    // GTA V save data is encrypted - decrypt it
    std::string savePath = std::string("save:") + dirName;
    std::vector<uint8_t> encryptedData;
    if (!FS::ReadFile(savePath, encryptedData)) {
        LOG_ERROR("SaveDataMount: Failed to read save file");
        return -1;
    }

    // Decrypt using PS5's AES key
    std::vector<uint8_t> decryptedData(encryptedData.size());
    if (!AES::Decrypt(encryptedData.data(), encryptedData.size(), decryptedData.data())) {
        LOG_ERROR("SaveDataMount: Failed to decrypt save data");
        return -1;
    }

    // Mount the decrypted save data
    if (!FS::Mount(mountPoint, decryptedData)) {
        LOG_ERROR("SaveDataMount: Failed to mount save data");
        return -1;
    }

    return 0;
}

int sceSaveDataUmount(const char* mountPoint) {
    if (!mountPoint) return -1;

    LOG_INFO("GTA V: SaveDataUmount called (%s)", mountPoint);
    return FS::Umount(mountPoint) ? 0 : -1;
}

int sceSaveDataInfoGet(int userId, const char* dirName, SceSaveDataInfo* info) {
    if (!dirName || !info) return -1;

    LOG_INFO("GTA V: SaveDataInfoGet called (userId=%d, dir=%s)", userId, dirName);

    // GTA V queries save data info
    std::string savePath = std::string("save:") + dirName;
    size_t size = FS::GetFileSize(savePath);
    if (size == 0) {
        return -1;
    }

    info->size = size;
    info->timestamp = FS::GetFileTimestamp(savePath);
    strncpy(info->dirName, dirName, sizeof(info->dirName));

    return 0;
}