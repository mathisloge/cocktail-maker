module;
#include <boost/asio/io_context.hpp>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

export module cm.core:async_scope;
import std;
import :logging;
import :io_scheduler;
import :task;

namespace cm {
namespace asio = boost::asio;

/**
 * Owns all fire-and-forget work of the Station and runs it on one io_context.
 *
 * It replaces detached spawning. Every spawned sender stays associated with this scope until it completes, so the
 * application can stop all of it and wait for it to finish before tearing down the objects it references.
 *
 * The underlying `stdexec::counting_scope` terminates the program if it is destroyed while work is still associated
 * with it, so `join()` must have completed before destruction whenever anything was spawned.
 */
export class AsyncScope
{
  public:
    explicit AsyncScope(asio::io_context& io_context)
        : scheduler_{io_context.get_executor()}
    {
    }

    AsyncScope(const AsyncScope&) = delete;
    AsyncScope(AsyncScope&&) = delete;
    AsyncScope& operator=(const AsyncScope&) = delete;
    AsyncScope& operator=(AsyncScope&&) = delete;
    ~AsyncScope() = default;

    /**
     * Starts `sender` on the io_context and associates it with this scope.
     *
     * It is thread-safe and can therefore also be called from the UI thread. Values are discarded and an error completion is
     * logged, because nobody waits for the result. Work spawned after `request_stop()` sees a stop request right away.
     */
    template <stdexec::sender Sender>
    void spawn(Sender&& sender)
    {
        stdexec::spawn(stdexec::starts_on(scheduler_, std::forward<Sender>(sender)) | stdexec::then([](auto&&...) noexcept {}) |
                           stdexec::upon_error([logger = logger_](auto&& error) noexcept { log_unhandled(logger, error); }),
                       scope_.get_token());
    }

    /// Requests every associated operation to stop. It must be called on the io_context's thread.
    void request_stop() noexcept
    {
        scope_.request_stop();
    }

    /// Completes once every associated operation has completed. It must be started on the io_context's thread.
    [[nodiscard]] Task<void> join();

    [[nodiscard]] IoScheduler scheduler() const noexcept
    {
        return scheduler_;
    }

    [[nodiscard]] IoScheduler::executor_type executor() const noexcept
    {
        return scheduler_.executor();
    }

  private:
    template <typename Error>
    static void log_unhandled(const log::Logger& logger, const Error& error) noexcept
    {
        try {
            if constexpr (std::same_as<Error, std::exception_ptr>) {
                std::rethrow_exception(error);
            }
            else {
                throw error;
            }
        }
        catch (const std::exception& ex) {
            SPDLOG_LOGGER_ERROR(logger, "Spawned operation failed: {}", ex.what());
        }
        catch (...) {
            SPDLOG_LOGGER_ERROR(logger, "Spawned operation failed with an unknown error.");
        }
    }

    log::Logger logger_{log::create_or_get("async_scope")};
    IoScheduler scheduler_;
    stdexec::counting_scope scope_;
};
} // namespace cm
