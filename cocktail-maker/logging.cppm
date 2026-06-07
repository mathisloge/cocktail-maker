module;
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

export module cm:logging;
import std;

namespace cm::log {

// ── Public type aliases ───────────────────────────────────────────────────────

export using Logger = std::shared_ptr<spdlog::logger>;
export using Level = spdlog::level::level_enum;

export namespace level {
constexpr auto trace = spdlog::level::trace;
constexpr auto debug = spdlog::level::debug;
constexpr auto info = spdlog::level::info;
constexpr auto warn = spdlog::level::warn;
constexpr auto error = spdlog::level::err;
constexpr auto critical = spdlog::level::critical;
constexpr auto off = spdlog::level::off;
} // namespace level

// ── Internal helpers ──────────────────────────────────────────────────────────

namespace detail {

template <typename... Args>
void log_impl(
    spdlog::logger& logger, const std::source_location& location, Level lvl, spdlog::format_string_t<Args...> fmt, Args&&... args)
{
    spdlog::source_loc loc{location.file_name(), static_cast<int>(location.line()), location.function_name()};
    logger.log(loc, lvl, fmt, std::forward<Args>(args)...);
}

} // namespace detail

// ── Logging context (singleton) ───────────────────────────────────────────────

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
        if (auto logger = spdlog::get(name)) {
            return logger;
        }
        auto logger = std::make_shared<spdlog::logger>(std::move(name), sinks_.begin(), sinks_.end());
        spdlog::initialize_logger(logger);
        spdlog::register_logger(logger);
        return logger;
    }

    void add_console_sink(Level lvl = level::trace, const std::string& pattern = "%+")
    {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
        sink->set_level(lvl);
        sink->set_pattern(pattern);
        sinks_.emplace_back(std::move(sink));
    }

    void add_file_sink(const std::string& filename, Level lvl = level::trace, bool truncate = false)
    {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_st>(filename, truncate);
        sink->set_level(lvl);
        sinks_.emplace_back(std::move(sink));
    }

    void add_sink(spdlog::sink_ptr sink)
    {
        sinks_.emplace_back(std::move(sink));
    }

    void add_rotating_file_sink(const std::string& filename,
                                std::size_t max_bytes,
                                std::size_t max_files,
                                Level lvl = level::trace)
    {
        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_st>(filename, max_bytes, max_files);
        sink->set_level(lvl);
        sinks_.emplace_back(std::move(sink));
    }

    void set_level(Level lvl)
    {
        spdlog::set_level(lvl);
    }

    void set_pattern(const std::string& pat)
    {
        spdlog::set_pattern(pat);
    }

    void flush_all()
    {
        spdlog::apply_all([](const Logger& l) { l->flush(); });
    }

    ~LoggingContext()
    {
        spdlog::shutdown();
    }

    LoggingContext(const LoggingContext&) = delete;
    LoggingContext& operator=(const LoggingContext&) = delete;

  private:
    LoggingContext()
    {
        spdlog::set_automatic_registration(false);
        spdlog::set_level(spdlog::level::debug);
        add_console_sink(level::debug);
    }

    std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks_;
};

// ── Free functions ────────────────────────────────────────────────────────────

export Logger create_or_get(std::string name)
{
    return LoggingContext::instance().get_logger(std::move(name));
}

export void set_level(Level lvl)
{
    LoggingContext::instance().set_level(lvl);
}

export void set_pattern(const std::string& pattern)
{
    LoggingContext::instance().set_pattern(pattern);
}

export void flush_all()
{
    LoggingContext::instance().flush_all();
}

export void add_file_sink(const std::string& filename, Level lvl = level::trace, bool truncate = false)
{
    LoggingContext::instance().add_file_sink(filename, lvl, truncate);
}

export void add_rotating_file_sink(const std::string& filename,
                                   std::size_t max_bytes,
                                   std::size_t max_files,
                                   Level lvl = level::trace)
{
    LoggingContext::instance().add_rotating_file_sink(filename, max_bytes, max_files, lvl);
}

export void add_sink(spdlog::sink_ptr sink)
{
    LoggingContext::instance().add_sink(std::move(sink));
}

// Usage:
//   cm::log::info(logger, "hello {}", name);
//   cm::log::info(*logger_ptr, "hello {}", name);   // deref shared_ptr
//   cm::log::info(logger_ptr, "hello {}", name);    // shared_ptr overload

#define CM_LOG_STRUCT(StructName, SpdlogLevel)                                                                                   \
    export template <typename... Args>                                                                                           \
    struct StructName                                                                                                            \
    {                                                                                                                            \
        /* logger& overload */                                                                                                   \
        StructName(spdlog::logger& logger,                                                                                       \
                   spdlog::format_string_t<Args...> fmt,                                                                         \
                   Args&&... args,                                                                                               \
                   std::source_location loc = std::source_location::current())                                                   \
        {                                                                                                                        \
            detail::log_impl(logger, loc, SpdlogLevel, fmt, std::forward<Args>(args)...);                                        \
        }                                                                                                                        \
        /* shared_ptr<logger> overload */                                                                                        \
        StructName(const Logger& logger,                                                                                         \
                   spdlog::format_string_t<Args...> fmt,                                                                         \
                   Args&&... args,                                                                                               \
                   std::source_location loc = std::source_location::current())                                                   \
        {                                                                                                                        \
            detail::log_impl(*logger, loc, SpdlogLevel, fmt, std::forward<Args>(args)...);                                       \
        }                                                                                                                        \
    };                                                                                                                           \
    template <typename... Args>                                                                                                  \
    StructName(spdlog::logger&, spdlog::format_string_t<Args...>, Args&&...) -> StructName<Args...>;                             \
    template <typename... Args>                                                                                                  \
    StructName(const Logger&, spdlog::format_string_t<Args...>, Args&&...)->StructName<Args...>

CM_LOG_STRUCT(trace, spdlog::level::trace);
CM_LOG_STRUCT(debug, spdlog::level::debug);
CM_LOG_STRUCT(info, spdlog::level::info);
CM_LOG_STRUCT(warn, spdlog::level::warn);
CM_LOG_STRUCT(error, spdlog::level::err);
CM_LOG_STRUCT(critical, spdlog::level::critical);

#undef CM_LOG_STRUCT

} // namespace cm::log
