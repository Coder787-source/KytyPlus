// SPDX-License-Identifier: MIT
#include "common/readOnlyFileSystem.h"
#include <mutex>

namespace Common {
namespace {
std::mutex mutex;
std::filesystem::path mount_root;
std::shared_ptr<ReadOnlyFileSystem> mounted;
}
void MountReadOnlyFileSystem(const std::filesystem::path& root, std::shared_ptr<ReadOnlyFileSystem> volume) {
    std::lock_guard lock(mutex);
    mount_root = std::filesystem::absolute(root).lexically_normal();
    mounted = std::move(volume);
}
std::shared_ptr<ReadOnlyFileSystem> FindReadOnlyFileSystem(const std::filesystem::path& path, std::string& relative) {
    std::lock_guard lock(mutex);
    if (!mounted) return {};
    // Do not let normalization turn an image path into an unrelated host path.
    auto absolute = std::filesystem::absolute(path);
    auto root = mount_root.begin();
    auto part = absolute.begin();
    for (; root != mount_root.end(); ++root, ++part) {
        if (part == absolute.end() || *root != *part) return {};
    }
    relative.clear();
    for (; part != absolute.end(); ++part) {
        auto value = part->generic_string();
        if (value == "." || value.empty()) continue;
        if (!relative.empty()) relative += '/';
        relative += value;
    }
    return mounted;
}
} // namespace Common
