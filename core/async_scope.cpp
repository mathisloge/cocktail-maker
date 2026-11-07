module;
#include <stdexec/execution.hpp>

module cm.core;

namespace cm {
Task<void> AsyncScope::join()
{
    co_await scope_.join();
}

Task<void> AsyncScope::stop_and_join()
{
    request_stop();
    co_await join();
}
} // namespace cm
