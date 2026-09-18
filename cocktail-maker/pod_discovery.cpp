module;
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

module cm:pod_discovery_impl;

import std;
import cm.core;
import :pod;
import :station_state;
import :pod_registry;
import :pod_discovery;

namespace cm {
namespace {
Task<void> run_pod(std::shared_ptr<IPod> pod, std::unique_ptr<PodState> pod_state, PodRegistry& pod_registry)
{
    auto logger{log::create_or_get("pod_discovery")};
    SPDLOG_LOGGER_INFO(logger, "New pod discovered. Running it now...");

    struct EntryGuard
    {
      private:
        PodRegistry& registry_;
        std::shared_ptr<IPod> pod_;

      public:
        explicit EntryGuard(PodRegistry& registry, std::shared_ptr<IPod> pod)
            : registry_{registry}
            , pod_{pod}
        {
            registry_.register_pod(pod_);
        }

        ~EntryGuard()
        {
            registry_.unregister_pod(pod_);
        }
    } cleanup_guard{pod_registry, pod};

    co_await pod->run(std::move(pod_state));
}
} // namespace

Task<void> discover_and_run_pods(std::unique_ptr<PodDiscovery> pod_discovery,
                                 std::shared_ptr<StationState> station_state,
                                 PodRegistry& pod_registry,
                                 AsyncScope& scope)
{
    auto logger{log::create_or_get("pod_discovery")};
    co_await pod_discovery->discover(scope, [&scope, station_state, &pod_registry](std::shared_ptr<IPod> pod) {
        if (pod != nullptr) {
            scope.spawn(run_pod(std::move(pod), station_state->create_pod_state(), pod_registry));
        }
    });
    SPDLOG_LOGGER_WARN(logger, "Pod discovery finished.");
}
} // namespace cm
