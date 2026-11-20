#include "common/logging/log.h"

#include "common/emulatorConfig.h"

#include <cstdio>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <memory>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

namespace Log {

static bool g_initialized = false;
static bool g_console     = false;
// Null when silent. Only Initialize sets it, before other threads start. Never destroyed, since
// threads keep logging while exit runs static destructors.
static spdlog::logger* g_logger = nullptr;
static spdlog::logger* g_graphics_diagnostic_logger = nullptr;
static std::atomic<uint64_t> g_graphics_diagnostic_records {0};
static constexpr uint64_t kGraphicsDiagnosticRecordLimit = 100000;

static void WriteStdout(std::string_view text, fmt::text_style style) {
	if (style == fmt::text_style {}) {
		std::fwrite(text.data(), 1, text.size(), stdout);
	} else {
		fmt::print(stdout, style, "{}", text);
	}
	std::fflush(stdout);
}

void Initialize() {
	g_initialized        = true;
	const auto direction = Config::GetPrintfDirection();
	const auto path      = Config::GetPrintfOutputFile();
	g_console            = direction == Config::LogDirection::Console;
	delete g_logger;
	g_logger = nullptr;

	spdlog::sink_ptr sink;
	if (g_console) {
		sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
	} else if (direction == Config::LogDirection::File && !path.empty()) {
		if (path.has_parent_path()) {
			std::filesystem::create_directories(path.parent_path());
		}
		const bool truncate = std::getenv(RelaunchedVariable) == nullptr;
		sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.native(), truncate);
	}
	if (sink != nullptr) {
		sink->set_formatter(std::make_unique<spdlog::pattern_formatter>(
		    "%v", spdlog::pattern_time_type::local, ""));
		g_logger = new spdlog::logger("kyty", std::move(sink));
	}

	// Independent opt-in log: does not change rendering or validation behaviour.
	delete g_graphics_diagnostic_logger;
	g_graphics_diagnostic_logger = nullptr;
	g_graphics_diagnostic_records.store(0, std::memory_order_relaxed);
	const auto* graphics_diag = std::getenv("KYTY_GRAPHICS_DIAG");
	if (graphics_diag != nullptr && std::strcmp(graphics_diag, "1") == 0) {
		const auto* requested_path = std::getenv("KYTY_GRAPHICS_DIAG_FILE");
		const std::filesystem::path diagnostic_path =
		    requested_path != nullptr && requested_path[0] != '\\0'
		        ? std::filesystem::path(requested_path)
		        : std::filesystem::path("kyty-graphics-diagnostics.log");
		if (diagnostic_path.has_parent_path()) {
			std::filesystem::create_directories(diagnostic_path.parent_path());
		}
		auto diagnostic_sink =
		    std::make_shared<spdlog::sinks::basic_file_sink_mt>(diagnostic_path.native(), true);
		diagnostic_sink->set_formatter(std::make_unique<spdlog::pattern_formatter>(
		    "%v", spdlog::pattern_time_type::local, ""));
		g_graphics_diagnostic_logger =
		    new spdlog::logger("kyty-graphics-diagnostics", std::move(diagnostic_sink));
		g_graphics_diagnostic_logger->log(spdlog::level::info,
		                                  "KYTY_GRAPHICS_DIAG session started; record_limit=100000");
	}
}

void Flush() {
	if (g_logger != nullptr) {
		g_logger->flush();
	}
	if (g_graphics_diagnostic_logger != nullptr) {
		g_graphics_diagnostic_logger->flush();
	}
}

bool IsSilent() {
	// Before Initialize the text goes to stdout.
	return g_initialized && g_logger == nullptr;
}

void Write(fmt::text_style style, std::string_view text) {
	if (!g_initialized) {
		WriteStdout(text, style);
		return;
	}
	if (g_logger == nullptr) {
		return;
	}
	if (g_console && style != fmt::text_style {}) {
		fmt::memory_buffer styled;
		fmt::format_to(std::back_inserter(styled), style, "{}", text);
		g_logger->log(spdlog::level::info, spdlog::string_view_t(styled.data(), styled.size()));
	} else {
		g_logger->log(spdlog::level::info, spdlog::string_view_t(text.data(), text.size()));
	}
}

void WriteGraphicsDiagnostic(std::string_view category, std::string_view text) {
	if (g_graphics_diagnostic_logger == nullptr) {
		return;
	}
	const auto record = g_graphics_diagnostic_records.fetch_add(1, std::memory_order_relaxed);
	if (record >= kGraphicsDiagnosticRecordLimit) {
		return;
	}
	g_graphics_diagnostic_logger->log(
	    spdlog::level::info,
	    fmt::format("[{:06}] [{}] {}", record, category, text));
}

void WriteToConsoleAndLog(std::string_view text) {
	if (g_initialized && !g_console) {
		WriteStdout(text, {});
	}
	Write({}, text);
	Flush();
}

} // namespace Log
