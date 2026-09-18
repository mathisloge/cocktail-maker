module;
export module cm.core:async_algorithms;
import std;
import :task;

namespace cm {
/**
 * Runs all `tasks` concurrently and waits for all of them.
 *
 * The first task that fails requests all of its siblings to stop. Once every task has finished, that first error is
 * rethrown. A stop request of the caller is forwarded to all tasks.
 */
export Task<void> join_all(std::vector<Task<void>> tasks);

/**
 * Runs all `tasks` concurrently and lets every one of them run to completion, even if some fail.
 *
 * Once every task has finished, the first error (in completion order) is rethrown. A stop request of the caller is
 * forwarded to all tasks.
 */
export Task<void> gather_all(std::vector<Task<void>> tasks);
} // namespace cm
