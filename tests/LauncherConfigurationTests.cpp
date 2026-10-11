#include "configuration.h"

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
