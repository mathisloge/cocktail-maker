module;
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

export module cm.core:logging;
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
    logger.log(std::move(loc), lvl, fmt, std::forward<Args>(args)...);
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
        add_sink(std::move(sink));
    }

    void add_file_sink(const std::string& filename, Level lvl = level::trace, bool truncate = false)
    {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_st>(filename, truncate);
        sink->set_level(lvl);
        add_sink(std::move(sink));
    }

    void add_sink(spdlog::sink_ptr sink)
    {
        sinks_.emplace_back(sink);
        spdlog::apply_all([sink](Logger logger) { logger->sinks().emplace_back(sink); });
    }

    void add_rotating_file_sink(const std::string& filename,
                                std::size_t max_bytes,
                                std::size_t max_files,
                                Level lvl = level::trace)
    {
        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_st>(filename, max_bytes, max_files);
        sink->set_level(lvl);
        add_sink(std::move(sink));
    }

    void set_level(Level lvl)
    {
        spdlog::set_level(lvl);
        for (auto&& s : sinks_) {
            s->set_level(lvl);
        }
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
} // namespace cm::log
