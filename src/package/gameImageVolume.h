// SPDX-License-Identifier: MIT
#pragma once
#include "common/readOnlyFileSystem.h"
#include <functional>

namespace Libs::Firmware {
std::shared_ptr<Common::ReadOnlyFileSystem> OpenGameImage(
    const std::filesystem::path& image, const std::filesystem::path& helper, std::string& error,
    const std::function<void(uint32_t, uint32_t)>& progress = {});
}
