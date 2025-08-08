// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/logging.hpp"
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace cm
{
LoggingContext &LoggingContext::instance()
{
    static LoggingContext ctx;
    return ctx;
}

LoggingContext::LoggingContext()
{
    spdlog::set_automatic_registration(false);
    spdlog::set_level(spdlog::level::level_enum{SPDLOG_ACTIVE_LEVEL});
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
    console_sink->set_level(spdlog::level::debug);
    console_sink->set_pattern("%+");

    sinks_.emplace_back(std::move(console_sink));
}

LoggingContext::~LoggingContext() = default;

std::shared_ptr<spdlog::logger> LoggingContext::create_logger(std::string name)
{
    auto logger = spdlog::get(name);
    if (logger == nullptr)
    {
        logger = std::make_shared<spdlog::logger>(std::move(name), sinks_.begin(), sinks_.end());
        spdlog::initialize_logger(logger);
        spdlog::register_logger(logger);
    }
    return logger;
}
} // namespace cm
