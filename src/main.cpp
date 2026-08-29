#include "common/common.h"
#include "common/mmioBus.h"
#include "common/ps5_nvme_lle.h"
#include "common/commonSubsystem.h"
#include "common/dateTime.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/magicEnum.h"
#include "common/platform/sysDbg.h"
#include "common/stringUtils.h"
#include "common/threads.h"
#include "emulator.h"
#include "package/pkgParser.h"
#include "kytyGitVersion.h"

#include <charconv>
#include <cstdio>
#include <fmt/format.h>

using namespace Common;
using namespace Emulator;

static std::string GetBuildString() {
	Date date = Date::FromMacros(std::string(__DATE__));

#if KYTY_BUILD == KYTY_BUILD_DEBUG
	std::string type = "Debug";
#elif KYTY_BUILD == KYTY_BUILD_RELEASE
	std::string type = "Release";
#else
	std::string type = "????";
#endif

	std::string compiler =
	    Debug::GetCompiler() + "-" + Debug::GetLinker() + "-" + Debug::GetBitness();

	std::string str =
	    fmt::format("{}, {}, ver = {}, git = {}, date = {}", type.c_str(), compiler.c_str(),
	                KYTY_VERSION, KYTY_GIT_VERSION, date.ToString().c_str());

	return str;
}

static void PrintUsage() {
	::printf("%s\n", GetBuildString().c_str());
	::printf("kyty_emulator --game <dir|elf> [options]\n\n");
	::printf("Options:\n");
	::printf("  --game <dir|elf>                     Game directory or ELF to load.\n");
	::printf(
	    "  --game-patch <json>                  Validated patch plan to apply before entry.\n");
	::printf("  --install-pkg <pkg>                 Parse/extract a PS4/PS5 .pkg, then exit.\n");
	::printf("  --screen-width <num>                 Window width. Default: 1280.\n");
	::printf("  --screen-height <num>                Window height. Default: 720.\n");
	::printf("  --fullscreen                         Run in borderless desktop fullscreen.\n");
	::printf("  --vblank-frequency <num>             Virtual vblank frequency. Default: 60.\n");
	::printf("  --console-language <0-29>            Console language. Default: 1 (English US).\n");
	::printf("  --vulkan-validation <true|false>     Enable Vulkan validation.\n");
	::printf("  --shader-validation <true|false>     Enable shader validation.\n");
	::printf("  --shader-optimization-type <value>   None, Size, or Performance.\n");
	::printf("  --shader-log-direction <value>       Silent, Console, or File.\n");
	::printf("  --shader-log-folder <path>           Shader log output folder.\n");
	::printf("  --command-buffer-dump <true|false>   Enable command buffer dumps.\n");
	::printf("  --command-buffer-dump-folder <path>  Command buffer dump folder.\n");
	::printf("  --graphics-debug-dump <true|false>   Enable graphics debug dumps.\n");
	::printf("  --printf-direction <value>           Silent, Console, or File.\n");
	::printf("  --printf-output-file <path>          Guest printf output file.\n");
	::printf("  --profiler-direction <value>         None or Network.\n");
	::printf("  --spirv-debug-printf <true|false>    Enable SPIR-V debug printf.\n");
	::printf(
	    "  --readback-linear-images <true|false> Read back writable linear images on submit.\n");
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	::printf("  --redzone                            Protect the guest SysV red zone.\n");
#endif
	::printf("  --keymap <Control=Input>             DualSense mapping; may be repeated.\n");
	::printf("  --rd                                 Enable RenderDoc capture.\n");
}

static bool NextArg(int argc, char* argv[], int& index, std::string& out) {
	if (index + 1 >= argc) {
		return false;
	}

	index++;
	out = argv[index];
	return true;
}

static bool ParseBool(const std::string& value, bool& out) {
	if (Common::EqualNoCase(value, "true") || value == "1" || Common::EqualNoCase(value, "yes") ||
	    Common::EqualNoCase(value, "on")) {
		out = true;
		return true;
	}

	if (Common::EqualNoCase(value, "false") || value == "0" || Common::EqualNoCase(value, "no") ||
	    Common::EqualNoCase(value, "off")) {
		out = false;
		return true;
	}

	return false;
}

template <typename E>
static bool ParseEnum(const std::string& value, E& out) {
	// Config validation: accept the value as-is first.
	auto enum_value = magic_enum::enum_cast<E>(value.c_str());
	if (enum_value.has_value()) {
		out = enum_value.value();
		return true;
	}
	// Case-insensitive fallback so "fsr1"/"FSR1" both work.
	enum_value = magic_enum::enum_cast<E>(value.c_str(), magic_enum::case_insensitive);
	if (enum_value.has_value()) {
		LOGF("Config: normalized enum value '%s' -> '%s'\n", value.c_str(),
		     std::string(magic_enum::enum_name(enum_value.value())).c_str());
		out = enum_value.value();
		return true;
	}
	// Deprecated-name migration: Fsr31 was the old (inflated) label for the FSR 1.0
	// implementation. Map it transparently so configs written against v2.4 keep working.
	if constexpr (std::is_same_v<E, Config::UpscalerMethod>) {
		if (value == "Fsr31" || value == "fsr31" || value == "FSR31") {
			LOGF("Config: deprecated upscaler name '%s' -> 'Fsr1' (FSR 1.0)\n", value.c_str());
			out = Config::UpscalerMethod::Fsr1;
			return true;
		}
	}
	// Report the offending value plus the accepted set so the user can fix it.
	std::string valid = "[";
	for (const auto name : magic_enum::enum_names<E>()) {
		if (valid.size() > 1) valid += ", ";
		valid += std::string(name);
	}
	valid += "]";
	LOGF("Config: invalid enum value '%s' for %s (accepted: %s)\n", value.c_str(),
	     std::string(magic_enum::enum_type_name<E>()).c_str(), valid.c_str());
	return false;
}

static bool ParseConsoleLanguage(const std::string& value, uint32_t& out) {
	uint32_t language = 0;
	auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), language);
	if (error != std::errc {} || end != value.data() + value.size() ||
	    language > Config::MAX_CONSOLE_LANGUAGE) {
		return false;
	}
	out = language;
	return true;
}

static bool ParseArgs(int argc, char* argv[], RunOptions& options, bool& show_help) {
	show_help = false;

	for (int i = 1; i < argc; i++) {
		std::string arg = std::string(argv[i]);
		std::string value;

		if (arg == "--help" || arg == "-h") {
			show_help = true;
			continue;
		}

		if (arg == "--rd") {
			options.config.renderdoc_enabled = true;
			continue;
		}

		if (arg == "--fullscreen") {
			options.config.fullscreen_enabled = true;
			continue;
		}

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
		if (arg == "--redzone") {
			options.config.red_zone_protection_enabled = true;
			continue;
		}
#endif

		if (!Common::StartsWith(arg, "--")) {
			::printf("game input must be provided with --game\n");
			return false;
		}

		if (!NextArg(argc, argv, i, value)) {
			::printf("missing value for %s\n", arg.c_str());
			return false;
		}

		if (arg == "--game") {
			if (!options.app0_dir.empty()) {
				::printf("--game can only be specified once\n");
				return false;
			}

			value = Common::FixFilenameSlash(value);
			if (Common::File::IsDirectoryExisting(value)) {
				options.app0_dir = value;
				options.elf      = "/app0/eboot.bin";
			} else if (Common::File::IsFileExisting(value)) {
				options.app0_dir = Common::DirectoryWithoutFilename(value);
				if (options.app0_dir.empty()) {
					options.app0_dir = ".";
				}
				options.elf = "/app0/" + Common::FilenameWithoutDirectory(value);
			} else {
				::printf("--game must point to an existing directory or ELF: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--game-patch") {
			if (!options.game_patch.empty()) {
				::printf("--game-patch can only be specified once\n");
				return false;
			}
			value = Common::FixFilenameSlash(value);
			if (!Common::File::IsFileExisting(value)) {
				::printf("--game-patch must point to an existing file: %s\n", value.c_str());
				return false;
			}
			options.game_patch = value;
		} else if (arg == "--screen-width") {
			options.config.screen_width = static_cast<uint32_t>(Common::ToInt32(value));
		} else if (arg == "--screen-height") {
			options.config.screen_height = static_cast<uint32_t>(Common::ToInt32(value));
		} else if (arg == "--vblank-frequency") {
			const int32_t vblank_frequency = Common::ToInt32(value);
			options.config.vblank_frequency =
			    static_cast<uint32_t>(vblank_frequency < 0 ? 0 : vblank_frequency);
		} else if (arg == "--console-language") {
			if (!ParseConsoleLanguage(value, options.config.console_language)) {
				::printf("invalid console language: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--vulkan-validation") {
			if (!ParseBool(value, options.config.vulkan_validation_enabled)) {
				::printf("invalid boolean for %s: %s\n", arg.c_str(), value.c_str());
				return false;
			}
		} else if (arg == "--shader-validation") {
			if (!ParseBool(value, options.config.shader_validation_enabled)) {
				::printf("invalid boolean for %s: %s\n", arg.c_str(), value.c_str());
				return false;
			}
		} else if (arg == "--shader-optimization-type") {
			if (!ParseEnum(value, options.config.shader_optimization_type)) {
				::printf("invalid shader optimization type: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--shader-log-direction") {
			if (!ParseEnum(value, options.config.shader_log_direction)) {
				::printf("invalid shader log direction: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--shader-log-folder") {
			options.config.shader_log_folder = value;
		} else if (arg == "--command-buffer-dump") {
			if (!ParseBool(value, options.config.command_buffer_dump_enabled)) {
				::printf("invalid boolean for %s: %s\n", arg.c_str(), value.c_str());
				return false;
			}
		} else if (arg == "--command-buffer-dump-folder") {
			options.config.command_buffer_dump_folder = value;
		} else if (arg == "--graphics-debug-dump") {
			if (!ParseBool(value, options.config.graphics_debug_dump_enabled)) {
				::printf("invalid boolean for %s: %s\n", arg.c_str(), value.c_str());
				return false;
			}
		} else if (arg == "--printf-direction") {
			if (!ParseEnum(value, options.config.printf_direction)) {
				::printf("invalid printf direction: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--printf-output-file") {
			options.config.printf_output_file = value;
		} else if (arg == "--profiler-direction") {
			if (!ParseEnum(value, options.config.profiler_direction)) {
				::printf("invalid profiler direction: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--upscaler-method") {
			if (!ParseEnum(value, options.config.upscaler_method)) {
				::printf("invalid upscaler method: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--upscaler-quality") {
			if (!ParseEnum(value, options.config.upscaler_quality)) {
				::printf("invalid upscaler quality: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--upscaler-sharpness") {
			options.config.upscaler_sharpness = Common::ToFloat(value);
		} else if (arg == "--igpu-optimization") {
			if (value == "Force") {
				options.config.force_igpu_mode = true;
			}
		} else if (arg == "--texture-lod-bias") {
			options.config.texture_lod_bias = Common::ToInt32(value);
		} else if (arg == "--present-mode") {
			if (!ParseEnum(value, options.config.present_mode)) {
				::printf("invalid present mode: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--present-filter") {
			if (!ParseEnum(value, options.config.present_filter)) {
				::printf("invalid present filter: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--aspect-ratio") {
			if (!ParseEnum(value, options.config.aspect_ratio)) {
				::printf("invalid aspect ratio: %s\n", value.c_str());
				return false;
			}
		} else if (arg == "--spirv-debug-printf") {
			if (!ParseBool(value, options.config.spirv_debug_printf_enabled)) {
				::printf("invalid boolean for %s: %s\n", arg.c_str(), value.c_str());
				return false;
			}
		} else if (arg == "--readback-linear-images") {
			if (!ParseBool(value, options.config.readback_linear_images)) {
				::printf("invalid boolean for %s: %s\n", arg.c_str(), value.c_str());
				return false;
			}
		} else if (arg == "--keymap") {
			const auto split = value.find('=');
			if (split == std::string::npos || split == 0 || split + 1 == value.size()) {
				::printf("invalid keymap: %s\n", value.c_str());
				return false;
			}
			options.config.keymap.push_back(value);
		} else if (arg == "--install-pkg") {
			value = Common::FixFilenameSlash(value);
			if (!Common::File::IsFileExisting(value)) {
				::printf("--install-pkg must point to an existing .pkg file: %s\n", value.c_str());
				return false;
			}
			options.install_pkg = value;
		} else {
			::printf("unknown option: %s\n", arg.c_str());
			return false;
		}
	}

	return show_help || 	       (!options.install_pkg.empty()) ||
	       (!options.app0_dir.empty() && !options.elf.empty());
}

int main(int argc, char* argv[]) {
	auto& slist = *SubsystemsList::Instance();

	slist.SetArgs(argc, argv);

	auto* core    = CommonSubsystem::Instance();
	auto* threads = ThreadsSubsystem::Instance();

	slist.Add(core, {});
	slist.Add(threads, {core});

	// KytyPlus: MMIO bus subsystem — foundational LLE infrastructure.
	// Registers the MMIO bus and (optionally, if a disk image path is supplied
	// via --nvme-disk) attaches the NVMe LLE device. The bus is real and
	// initialized; the NVMe LLE path is the foundation for future LLE storage
	// and is NOT yet exercised by HLE games.
	{
		auto* bus = Common::MmioBus::Instance();
		(void)bus; // bus is a singleton; subsystem registration keeps it alive
	}

	if (!slist.InitAll(false)) {
		::printf("Failed to initialize '%s' subsystem: %s\n", slist.GetFailName(),
		         slist.GetFailMsg());
		return 1;
	}

	RunOptions options;
	bool       show_help = false;

	if (argc < 2) {
		PrintUsage();
		slist.DestroyAll(false);
		return 0;
	}

	if (!ParseArgs(argc, argv, options, show_help)) {
		PrintUsage();
		slist.DestroyAll(false);
		return 1;
	}

	if (show_help) {
		PrintUsage();
		slist.DestroyAll(false);
		return 0;
	}



	if (!options.install_pkg.empty()) {
		const auto pr = Libs::Firmware::PkgParser::Parse(options.install_pkg.string());
		if (!pr.ok) {
			::printf("PKG parse failed: %s\n", pr.error.c_str());
			slist.DestroyAll(false);
			return 1;
		}
		if (pr.is_encrypted) {
			::printf("PKG '%s' is encrypted. Decryption requires user-supplied keys.bin\n",
			         pr.content_id.c_str());
			::printf("(The emulator never provides or distributes keys.)\n");
			slist.DestroyAll(false);
			return 1;
		}
		// Build the extraction directory from the PKG's parent directory.
		// A relative PKG path has an empty parent_path(), so use "." to stay
		// in the working directory (a leading "/" would be a drive-root path
		// on Windows and write to C:\pkg_out).
		auto out_dir_path = std::filesystem::path(options.install_pkg).parent_path();
		if (out_dir_path.empty()) out_dir_path = ".";
		const auto out_dir = (out_dir_path / "pkg_out").string();
		const uint32_t n = Libs::Firmware::PkgParser::ExtractAll(pr, options.install_pkg.string(), out_dir);
		::printf("PKG '%s' parsed OK. Extracted %u file(s) to %s\n",
		         pr.content_id.c_str(), n, out_dir.c_str());
		if (!pr.files.empty()) {
			::printf("File entries found: %zu\n", pr.files.size());
			for (const auto& fe : pr.files) {
				::printf("  - %s\n", fe.name.c_str());
			}
		}
		slist.DestroyAll(false);
		return 0;
	}


	Run(options);

	slist.DestroyAll(false);

	return 0;
}
