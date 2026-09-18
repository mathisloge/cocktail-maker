module;
#include <stdexec/execution.hpp>

module cm.core:async_algorithms_impl;
import std;
import :task;
import :async_algorithms;

namespace cm {
namespace {
enum class SiblingPolicy
{
    stop_on_error,
    run_to_completion,
};

/// Runs all `tasks` concurrently on the current scheduler and waits for every one of them, even if the caller is stopped.
Task<std::exception_ptr> run_all(std::vector<Task<void>> tasks, SiblingPolicy policy)
{
    const auto scheduler = co_await stdexec::read_env(stdexec::get_start_scheduler);
    const auto parent_token = co_await stdexec::read_env(stdexec::get_stop_token);

    std::exception_ptr first_error;
    stdexec::inplace_stop_source children_stop;
    const auto forward_parent_stop = [&children_stop]() noexcept { children_stop.request_stop(); };
    const stdexec::stop_callback_for_t<decltype(parent_token), decltype(forward_parent_stop)> parent_stop_callback{
        parent_token, forward_parent_stop};

    stdexec::simple_counting_scope scope;
    const auto children_env = stdexec::env{stdexec::prop{stdexec::get_stop_token, children_stop.get_token()},
                                           stdexec::prop{stdexec::get_start_scheduler, scheduler}};

    for (auto& task : tasks) {
        stdexec::spawn(std::move(task) |
                           stdexec::upon_error([&first_error, &children_stop, policy](std::exception_ptr error) noexcept {
                               if (!first_error) {
                                   first_error = std::move(error);
                               }
                               if (policy == SiblingPolicy::stop_on_error) {
                                   children_stop.request_stop();
                               }
                           }),
                       scope.get_token(),
                       children_env);
    }

    // The children reference this frame, so the join is awaited even if the caller is stopped.
    co_await scope.join();

    if (!first_error && parent_token.stop_requested()) {
        co_yield stdexec::with_stopped{};
    }
    co_return first_error;
}
} // namespace

Task<void> join_all(std::vector<Task<void>> tasks)
{
    if (auto error = co_await run_all(std::move(tasks), SiblingPolicy::stop_on_error)) {
        std::rethrow_exception(error);
    }
}

Task<void> gather_all(std::vector<Task<void>> tasks)
{
    if (auto error = co_await run_all(std::move(tasks), SiblingPolicy::run_to_completion)) {
        std::rethrow_exception(error);
    }
}
} // namespace cm
