module;
#include <stdexec/execution.hpp>

module cm:pod_registry_impl;
import std;
import cm.core;
import :pod;
import :pod_algorithms;
import :pod_registry;

namespace cm {
Task<void> PodRegistry::force_safe_state_all_pods() const
{
    auto locked = pods_ | std::views::transform([](const std::weak_ptr<IPod>& p) { return p.lock(); }) |
                  std::views::filter([](const std::shared_ptr<IPod>& p) { return p != nullptr; });

    co_await force_safe_state_all(locked);
}
} // namespace cm
