module;
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

export module cm:logging;
import std;

namespace cm::log {

export using Logger = std::shared_ptr<spdlog::logger>;

class LoggingContext
{
  public:
    static LoggingContext& instance()
    {
        static LoggingContext ctx;
        return ctx;
    }

    Logger get_logger(std::string name)
    {
        auto logger = spdlog::get(name);
        if (logger == nullptr) {
            logger = std::make_shared<spdlog::logger>(std::move(name), sinks_.begin(), sinks_.end());
            spdlog::initialize_logger(logger);
            spdlog::register_logger(logger);
        }
        return logger;
    }

    ~LoggingContext() = default;

  private:
    LoggingContext()
    {
        spdlog::set_automatic_registration(false);
        spdlog::set_level(spdlog::level::debug);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
        console_sink->set_level(spdlog::level::debug);
        console_sink->set_pattern("%+");

        sinks_.emplace_back(std::move(console_sink));
    }

  private:
    std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks_;
};

template <typename... Args>
void log_impl(spdlog::logger& logger,
              const std::source_location& location,
              spdlog::level::level_enum level,
              spdlog::format_string_t<Args...> fmt,
              Args&&... args)
{
    spdlog::source_loc spdlog_location{location.file_name(), static_cast<int>(location.line()), location.function_name()};
    logger.log(std::move(spdlog_location), level, std::move(fmt), std::forward<Args>(args)...);
}

export template <typename... Args>
struct trace
{
    trace(spdlog::logger& logger,
          spdlog::format_string_t<Args...> fmt,
          Args&&... args,
          std::source_location location = std::source_location::current())
    {
        log_impl(logger, location, spdlog::level::trace, std::move(fmt), std::forward<Args>(args)...);
    }
};

template <typename... Args>
trace(spdlog::logger&, spdlog::format_string_t<Args...>, Args&&...) -> trace<Args...>;

export template <typename... Args>
struct debug
{
    debug(spdlog::logger& logger,
          spdlog::format_string_t<Args...> fmt,
          Args&&... args,
          std::source_location location = std::source_location::current())
    {
        log_impl(logger, location, spdlog::level::debug, std::move(fmt), std::forward<Args>(args)...);
    }
};

template <typename... Args>
debug(spdlog::logger&, spdlog::format_string_t<Args...>, Args&&...) -> debug<Args...>;

export template <typename... Args>
struct info
{
    info(spdlog::logger& logger,
         spdlog::format_string_t<Args...> fmt,
         Args&&... args,
         std::source_location location = std::source_location::current())
    {
        log_impl(logger, location, spdlog::level::info, std::move(fmt), std::forward<Args>(args)...);
    }
};

template <typename... Args>
info(spdlog::logger&, spdlog::format_string_t<Args...>, Args&&...) -> info<Args...>;

export template <typename... Args>
struct warn
{
    warn(spdlog::logger& logger,
         spdlog::format_string_t<Args...> fmt,
         Args&&... args,
         std::source_location location = std::source_location::current())
    {
        log_impl(logger, location, spdlog::level::warn, std::move(fmt), std::forward<Args>(args)...);
    }
};

template <typename... Args>
warn(spdlog::logger&, spdlog::format_string_t<Args...>, Args&&...) -> warn<Args...>;

export template <typename... Args>
struct error
{
    error(spdlog::logger& logger,
          spdlog::format_string_t<Args...> fmt,
          Args&&... args,
          std::source_location location = std::source_location::current())
    {
        log_impl(logger, location, spdlog::level::err, std::move(fmt), std::forward<Args>(args)...);
    }
};

template <typename... Args>
error(spdlog::logger&, spdlog::format_string_t<Args...>, Args&&...) -> error<Args...>;

export template <typename... Args>
struct critical
{
    critical(spdlog::logger& logger,
             spdlog::format_string_t<Args...> fmt,
             Args&&... args,
             std::source_location location = std::source_location::current())
    {
        log_impl(logger, location, spdlog::level::err, std::move(fmt), std::forward<Args>(args)...);
    }
};

template <typename... Args>
critical(spdlog::logger&, spdlog::format_string_t<Args...>, Args&&...) -> critical<Args...>;

export Logger create_or_get(std::string name)
{
    return LoggingContext::instance().get_logger(std::move(name));
}

} // namespace cm::log
