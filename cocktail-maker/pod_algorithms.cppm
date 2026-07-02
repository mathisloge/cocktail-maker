module;
#include <boost/cobalt/gather.hpp>
#include <boost/cobalt/task.hpp>
#include <libassert/assert-macros.hpp>

export module cm:pod_algorithms;
import std;
import libassert;
import :pod;

namespace cm {
export template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, std::shared_ptr<IPod>>
cobalt::task<void> force_safe_state_all(R&& pods)
{
    std::vector<cobalt::task<void>> tasks;
    if constexpr (std::ranges::sized_range<R>) {
        tasks.reserve(std::ranges::size(pods));
    }
    for (const auto& pod : pods) {
        ASSERT(pod != nullptr, "IPod in range must not be null.");
        tasks.emplace_back(pod->force_safe_state());
    }

    auto results = co_await cobalt::gather(std::move(tasks));

    std::vector<std::exception_ptr> errors;
    for (auto& result : results) {
        if (result.has_error()) {
            errors.push_back(result.error());
        }
    }
    if (!errors.empty()) {
        std::rethrow_exception(errors.front());
    }
}
} // namespace cm
