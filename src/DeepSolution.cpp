#include "Application.h"

#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

int main()
{
	auto console_sink = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
	console_sink->set_level(spdlog::level::warn);

	auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("log", 1024 * 1024 * 3, 3);
	file_sink->set_level(spdlog::level::trace);

	spdlog::sinks_init_list sinks{ console_sink,file_sink };

	auto logger = std::make_shared<spdlog::logger>("Global Logger", sinks.begin(), sinks.end());
	logger->flush_on(spdlog::level::err);
	spdlog::register_logger(logger);

	Application app(1920, 1080);
	app.run();
}
