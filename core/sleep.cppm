module;
#include <boost/asio/any_io_executor.hpp>

export module cm.core:sleep;
import std;
import :task;

namespace cm {
/// Waits for `duration`. A stop request completes it with `set_stopped`.
export [[nodiscard]] Task<void> async_sleep(boost::asio::any_io_executor executor, std::chrono::steady_clock::duration duration);

/// Waits for a stop request, which completes it with `set_stopped`. Nothing else ends it.
export [[nodiscard]] Task<void> wait_until_stopped(boost::asio::any_io_executor executor);
} // namespace cm
