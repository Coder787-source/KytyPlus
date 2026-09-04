#pragma once

#include <cstdint>
#include "kyty_expected.hpp"
#include <memory>
#include <string>
#include <vector>

namespace KytyPS5::Loader {

struct LoadImage {
	uint64_t entry_point = 0;
};

class ElfLoader {
public:
	ElfLoader() = default;

	kyty::expected<LoadImage, std::string> Load(const std::string& /*image_path*/) {
		LoadImage image;
		image.entry_point = 0;
		return image;
	}
};

} // namespace KytyPS5::Loader
