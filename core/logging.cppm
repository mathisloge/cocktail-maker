module;
#include <spdlog/logger.h>
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
        // The UI thread and the execution context both create loggers, and registering a name twice throws.
        const std::scoped_lock lock{mutex_};
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
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sink->set_level(lvl);
        sink->set_pattern(pattern);
        add_sink(std::move(sink));
    }

    void add_sink(spdlog::sink_ptr sink)
    {
        const std::scoped_lock lock{mutex_};
        sinks_.emplace_back(sink);
        spdlog::apply_all([sink](Logger logger) { logger->sinks().emplace_back(sink); });
    }

    void set_level(Level lvl)
    {
        const std::scoped_lock lock{mutex_};
        spdlog::set_level(lvl);
        for (auto&& s : sinks_) {
            s->set_level(lvl);
        }
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

    std::mutex mutex_;
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

export void add_sink(spdlog::sink_ptr sink)
{
    LoggingContext::instance().add_sink(std::move(sink));
}
} // namespace cm::log
