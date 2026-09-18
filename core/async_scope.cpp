module;
#include <stdexec/execution.hpp>

module cm.core:async_scope_impl;
import :async_scope;

namespace cm {
Task<void> AsyncScope::join()
{
    co_await scope_.join();
}
} // namespace cm
