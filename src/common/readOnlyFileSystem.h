// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Common {

class ReadOnlyFileSystem {
public:
    struct Entry { std::string name; bool is_file; };
    virtual ~ReadOnlyFileSystem() = default;
    virtual bool Stat(const std::string& path, uint64_t& size, bool& directory) const = 0;
    virtual bool Read(const std::string& path, uint64_t offset, void* data, uint32_t length) = 0;
    virtual std::vector<Entry> List(const std::string& path) const = 0;
};

// The mount root is a virtual host path, not an extracted directory. A shared
// reference keeps open handles valid if a mount is subsequently replaced.
void MountReadOnlyFileSystem(const std::filesystem::path& root, std::shared_ptr<ReadOnlyFileSystem> volume);
std::shared_ptr<ReadOnlyFileSystem> FindReadOnlyFileSystem(const std::filesystem::path& path, std::string& relative);

} // namespace Common
