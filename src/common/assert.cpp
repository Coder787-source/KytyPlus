#include "common/assert.h"

#include "common/logging/log.h"
#include "common/subsystems.h"
#include "kytyGitVersion.h"

#include <cstdio>
#include <cstdlib>
#include <fmt/format.h>
#include <string>

namespace Common {

static std::string BuildFatalReport(const char* title, std::string_view text, const char* file,
                                    int line) {
	return fmt::format("--- Build ---\n{}\n{}\n{} in {}:{}\n", KYTY_BUILD_LABEL, title, text, file,
	                   line);
}

static int DbgReport(const char* title, std::string_view text, const char* file, int line) {
	Log::WriteFatal(BuildFatalReport(title, text, file, line));
	Subsystems::EmergencyShutdownActive();
	return 1;
}

int DbgExitIfHandler(const char* expr, const char* file, int line) {
	return DbgReport("--- Fatal Error ---", fmt::format("Error: condition ({}) is true", expr),
	                 file, line);
}

int DbgNotImplementedHandler(const char* expr, const char* file, int line) {
	return DbgReport("--- Fatal Error ---", fmt::format("Not implemented ({})", expr), file, line);
}

int DbgNotImplementedHandler(const char* expr, const char* file, int line, std::string_view msg) {
	return DbgReport("--- Fatal Error ---",
	                  fmt::format("Not implemented: {} (condition: {})", msg, expr), file, line);
}

static int DbgReportSoft(const char* title, std::string_view text, const char* file, int line) {
	// Soft (non-fatal) variant: log through the normal channel with an explicit
	// "ignored" marker so tester logs still record the gap, but do NOT shut down
	// subsystems or terminate. Returns 1 ("fired") so the guard macro short-circuits.
	// Not a noreturn path: execution continues in the caller.
	Log::Write(fmt::format("{}\n{} in {}:{} (soft: ignored, rendering continues)\n", title,
	                       text, file, line));
	return 1;
}

int DbgSoftNotImplementedHandler(const char* expr, const char* file, int line,
                                 std::string_view msg) {
	const auto text = msg.empty() ? fmt::format("Unimplemented ({})", expr)
	                              : fmt::format("Unimplemented: {} (condition: {})", msg, expr);
	return DbgReportSoft("--- Ignored ---", text, file, line);
}

int DbgSoftExitHandler(const char* file, int line, std::string_view text) {
	return DbgReportSoft("--- Ignored ---", text, file, line);
}

int DbgExitHandler(const char* file, int line, std::string_view text) {
	Log::WriteFatal(BuildFatalReport("--- Error ---", text, file, line));
	return 1;
}

int DbgExitHandler(const char* file, int line, fmt::text_style style, std::string_view text) {
	Log::WriteFatal(style, BuildFatalReport("--- Error ---", text, file, line));
	return 1;
}

void DbgExit(int status) {
	Subsystems::EmergencyShutdownActive();
	std::fflush(nullptr);
	std::_Exit(status);
}

} // namespace Common
