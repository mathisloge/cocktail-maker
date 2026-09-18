module;
#include <stdexec/execution.hpp>

export module cm.core:task;

namespace cm {
/**
 * The coroutine type used for all asynchronous Station code.
 *
 * The coroutine body only starts once the task is awaited or connected. After every `co_await` it resumes on the scheduler
 * it was started on, and it forwards stop requests of its parent. A stop request does not surface as an exception. The
 * awaited operation completes with `set_stopped`, the coroutine frame is unwound and code after that `co_await` does not
 * run. Cleanup that must also happen on cancellation therefore belongs into RAII guards.
 */
export template <typename T = void>
using Task = stdexec::task<T>;
} // namespace cm
