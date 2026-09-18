module;
#include <libassert/assert-macros.hpp>

export module cm:pod_algorithms;
import std;
import libassert;
import cm.core;
import :pod;

namespace cm {
/**
 * Brings every pod in `pods` into its safe state concurrently.
 *
 * Every pod is commanded even if another one fails, since a pod left running is a safety concern. Once all are done, the
 * first error is rethrown.
 */
export template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, std::shared_ptr<IPod>>
Task<void> force_safe_state_all(R&& pods)
{
    std::vector<Task<void>> tasks;
    if constexpr (std::ranges::sized_range<R>) {
        tasks.reserve(std::ranges::size(pods));
    }
    for (const auto& pod : pods) {
        ASSERT(pod != nullptr, "IPod in range must not be null.");
        tasks.emplace_back(pod->force_safe_state());
    }

    co_await gather_all(std::move(tasks));
}
} // namespace cm
