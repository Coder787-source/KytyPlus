#include "configuration.h"
#include "common.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

static void Check(bool condition, const char* message) {
	if (!condition) { std::fprintf(stderr, "%s", message); std::abort(); }
}

int main(int argc, char** argv) {
	QCoreApplication app(argc, argv);
	Check(IsGameImagePath("C:/games/My Game.exfat") && IsGameImagePath("C:/games/My Game.EXFAT"),
	      "exFAT image paths are case-insensitive");
	Check(IsGameImagePath("game.ffpfsc") && IsGameImagePath("game.FFPFSC"),
	      "compressed image paths remain supported");
	Check(!IsGameImagePath("game.exfat.bak") && !IsGameImagePath("eboot.bin") &&
	          !IsGameImagePath("game.pkg") && !IsGameImagePath("exfat"),
	      "non-image paths are not treated as game images");
	QTemporaryDir temp;
	Check(temp.isValid(), "temporary settings directory");
	QSettings settings(temp.filePath("settings.ini"), QSettings::IniFormat);
	Configuration fresh;
	Check(fresh.upscaler_method == Configuration::UpscalerMethod::Fsr1 &&
	          fresh.upscaler_quality == Configuration::UpscalerQuality::Performance &&
	          fresh.upscaler_sharpness == 0.3f, "fresh launcher FSR defaults");
	fresh.ReadSettings(&settings);
	Check(fresh.upscaler_method == Configuration::UpscalerMethod::Fsr1 &&
	          fresh.upscaler_quality == Configuration::UpscalerQuality::Performance &&
	          fresh.upscaler_sharpness == 0.3f, "missing settings preserve defaults");
	fresh.upscaler_method = Configuration::UpscalerMethod::Off;
	fresh.upscaler_quality = Configuration::UpscalerQuality::Balanced;
	fresh.upscaler_sharpness = 0.7f;
	fresh.WriteSettings(&settings);
	settings.sync();
	Configuration loaded;
	loaded.ReadSettings(&settings);
	Check(loaded.upscaler_method == Configuration::UpscalerMethod::Off &&
	          loaded.upscaler_quality == Configuration::UpscalerQuality::Balanced &&
	          loaded.upscaler_sharpness == 0.7f, "explicit user overrides preserved");
	Configuration copied;
	copied.CopyEmulatorSettingsFrom(loaded);
	Check(copied.upscaler_method == Configuration::UpscalerMethod::Off,
	      "copied per-game settings preserve override");
	std::puts("Launcher configuration regression tests passed");
	return 0;
}
