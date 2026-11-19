#include "common/logging/log.h"

#include "common/emulatorConfig.h"

#include <cstdio>
#include <cstdlib>
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
static spdlog::logger* g_mesh_logger = nullptr;

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

	// Keep the Mesh investigation trace separate from the regular emulator log. Put it beside
	// the configured printf log when available, otherwise in the process working directory.
	delete g_mesh_logger;
	g_mesh_logger = nullptr;
	try {
		const auto mesh_path = path.empty() ? std::filesystem::path("kyty-mesh-investigation.txt")
		                                    : path.parent_path() / "kyty-mesh-investigation.txt";
		if (mesh_path.has_parent_path()) {
			std::filesystem::create_directories(mesh_path.parent_path());
		}
		const bool truncate = std::getenv(RelaunchedVariable) == nullptr;
		auto mesh_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(mesh_path.native(), truncate);
		mesh_sink->set_formatter(std::make_unique<spdlog::pattern_formatter>(
		    "%v", spdlog::pattern_time_type::local, ""));
		g_mesh_logger = new spdlog::logger("kyty-mesh-investigation", std::move(mesh_sink));
		const auto* layer = std::getenv("KYTY_MESH_LAYER");
		const auto* fast_first = std::getenv("KYTY_PIPELINE_FAST_FIRST");
		WriteMeshDiagnostic(fmt::sprintf(
		    "KYTY_MESH_DIAG_SESSION start layer=%s pipeline_fast_first=%s file=%s\n",
		    layer != nullptr ? layer : "(unset)", fast_first != nullptr ? fast_first : "(unset)",
		    mesh_path.generic_string()));
	} catch (...) {
		// Diagnostic logging must never prevent the emulator from starting.
		g_mesh_logger = nullptr;
	}
}

void Flush() {
	if (g_logger != nullptr) {
		g_logger->flush();
	}
	if (g_mesh_logger != nullptr) {
		g_mesh_logger->flush();
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

void WriteToConsoleAndLog(std::string_view text) {
	if (g_initialized && !g_console) {
		WriteStdout(text, {});
	}
	Write({}, text);
	Flush();
}

void WriteMeshDiagnostic(std::string_view text) {
	if (g_mesh_logger != nullptr) {
		g_mesh_logger->log(spdlog::level::info, spdlog::string_view_t(text.data(), text.size()));
		g_mesh_logger->flush();
	} else {
		WriteToConsoleAndLog(text);
	}
}

} // namespace Log
