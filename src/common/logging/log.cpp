
#include "common/logging/log.h"

#include "common/assert.h"
#include "common/emulatorConfig.h"
#include "common/stringUtils.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fmt/format.h>
#include <memory>
#include <mutex>
#include <spdlog/formatter.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <string>
#include <string_view>

namespace {

class RawFormatter final: public spdlog::formatter {
public:
	void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override {
		const std::string_view payload(msg.payload.data(), msg.payload.size());
		dest.append(payload.data(), payload.data() + payload.size());
	}

	std::unique_ptr<spdlog::formatter> clone() const override {
		return std::make_unique<RawFormatter>();
	}
};

std::shared_ptr<spdlog::logger> MakeLogger(std::string name, spdlog::sink_ptr sink) {
	sink->set_formatter(std::make_unique<RawFormatter>());

	auto logger = std::make_shared<spdlog::logger>(std::move(name), std::move(sink));
	logger->set_level(spdlog::level::trace);
	return logger;
}

std::shared_ptr<spdlog::logger> MakeFileLogger(std::string                  name,
                                               const std::filesystem::path& path) {
	const auto parent = path.parent_path();
	if (!parent.empty()) {
		std::filesystem::create_directories(parent);
	}

	auto sink =
	    std::make_shared<spdlog::sinks::basic_file_sink_mt>(Common::PathToString(path), true);
	return MakeLogger(std::move(name), std::move(sink));
}

static bool HasStyle(fmt::text_style style) {
	return style != fmt::text_style {};
}

void WriteStdout(std::string_view text, fmt::text_style style = {}) {
	if (HasStyle(style)) {
		fmt::print(stdout, style, "{}", text);
	} else {
		std::fwrite(text.data(), 1, text.size(), stdout);
	}
	std::fflush(stdout);
}

} // namespace

namespace Log {

static bool                            g_initialized = false;
static Direction                       g_direction   = Direction::Console;
static std::filesystem::path           g_output_file;
static std::mutex                      g_logger_mutex;
static std::shared_ptr<spdlog::logger> g_logger;
static std::string                     g_last_message;
static uint64_t                        g_repeat_count = 0;

void Flush() {
	if (g_logger != nullptr) {
		g_logger->flush();
	}
}

// Writes out a pending repeat counter as a notice line.
// Must be called with g_logger_mutex held.
static void FlushPendingRepeat() {
	if (g_repeat_count == 0 || g_logger == nullptr) {
		return;
	}
	const auto notice = fmt::format("[previous message repeated {} times]\n", g_repeat_count);
	g_logger->log(spdlog::level::info, spdlog::string_view_t(notice.data(), notice.size()));
	g_repeat_count = 0;
}

static void SetupLogger() {
	std::lock_guard lock(g_logger_mutex);
	Flush();
	FlushPendingRepeat();
	g_logger.reset();
	g_last_message.clear();
	g_repeat_count = 0;

	switch (g_direction) {
		case Direction::Silent:
			g_logger = MakeLogger("kyty", std::make_shared<spdlog::sinks::null_sink_mt>());
			break;
		case Direction::Console:
			g_logger = MakeLogger("kyty", std::make_shared<spdlog::sinks::stdout_sink_mt>());
			break;
		case Direction::File:
			if (!g_output_file.empty()) {
				g_logger = MakeFileLogger("kyty", g_output_file);
			}
			break;
	}
}

static void WriteImpl(std::string_view text, fmt::text_style style = {}) {
	if (text.empty()) {
		return;
	}

	if (!g_initialized) {
		WriteStdout(text, style);
		return;
	}

	if (g_direction == Direction::Silent) {
		return;
	}

	std::lock_guard lock(g_logger_mutex);

	auto logger = g_logger;
	if (logger == nullptr) {
		return;
	}

	// Collapse consecutive duplicate messages into a repeat counter instead of
	// writing them out: a hot-path stub can emit the same lines millions of
	// times per second, which floods the log file (multi-GB files that hide
	// the useful lines) and wastes disk I/O. Nothing is lost: the first
	// occurrence is always written and the counter is flushed as a notice
	// line as soon as a different message arrives or the logger shuts down.
	if (g_last_message == text) {
		++g_repeat_count;
		return;
	}

	FlushPendingRepeat();

	g_last_message.assign(text.data(), text.size());

	if (g_direction == Direction::Console && HasStyle(style)) {
		const auto styled = fmt::format(style, "{}", text);
		logger->log(spdlog::level::info, spdlog::string_view_t(styled.data(), styled.size()));
	} else {
		logger->log(spdlog::level::info, spdlog::string_view_t(text.data(), text.size()));
	}
}

void WriteFatal(std::string_view text) {
	if (g_direction == Direction::Silent || !g_initialized) {
		WriteStdout(text);
	} else {
		WriteStdout(text);
		WriteImpl(text);
	}
	Flush();
}

void WriteFatal(fmt::text_style style, std::string_view text) {
	if (g_direction == Direction::Silent || !g_initialized) {
		WriteStdout(text, style);
	} else {
		WriteStdout(text, style);
		WriteImpl(text, style);
	}
	Flush();
}

KYTY_SUBSYSTEM_INIT(Log) {
	g_initialized = true;
	switch (Config::GetPrintfDirection()) {
		case Config::OutputDirection::Silent: g_direction = Direction::Silent; break;
		case Config::OutputDirection::Console: g_direction = Direction::Console; break;
		case Config::OutputDirection::File: g_direction = Direction::File; break;
	}
	g_output_file =
	    (g_direction == Direction::File ? Config::GetPrintfOutputFile() : std::filesystem::path {});
	SetupLogger();
}

KYTY_SUBSYSTEM_UNEXPECTED_SHUTDOWN(Log) {
	Flush();
	std::lock_guard lock(g_logger_mutex);
	FlushPendingRepeat();
	g_logger.reset();
}

KYTY_SUBSYSTEM_DESTROY(Log) {
	Flush();
	std::lock_guard lock(g_logger_mutex);
	FlushPendingRepeat();
	g_logger.reset();
}

Direction GetDirection() {
	EXIT_IF(!g_initialized);
	return g_direction;
}

bool IsSilent() {
	// Before init LOGF must keep writing to stdout, so report non-silent.
	return g_initialized && g_direction == Direction::Silent;
}

void Write(std::string_view text) {
	WriteImpl(text);
}

void Write(fmt::text_style style, std::string_view text) {
	WriteImpl(text, style);
}

} // namespace Log
