module;
#include <boost/cobalt.hpp>
#include <spdlog/spdlog.h>

export module cm:pod_discovery;

import std;
import cm.core;
import :pod;
import :station_state;
import :pod_registry;

namespace cm {

export class PodDiscovery
{
  public:
    virtual ~PodDiscovery() = default;
    virtual cobalt::generator<std::shared_ptr<IPod>> discover() = 0;
};

cobalt::promise<void> run_pod(std::shared_ptr<IPod> pod, std::unique_ptr<PodState> pod_state, PodRegistry& pod_registry)
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

export cobalt::task<void> discover_and_run_pods(std::unique_ptr<PodDiscovery> pod_discovery,
                                                StationState& station_state,
                                                PodRegistry& pod_registry)
{
    auto logger{log::create_or_get("pod_discovery")};
    co_await cobalt::with(cobalt::wait_group{},
                          [logger, pod_discovery = std::move(pod_discovery), &station_state, &pod_registry](
                              cobalt::wait_group& pod_sessions) -> cobalt::promise<void> {
                              auto discover_pod = pod_discovery->discover();
                              BOOST_COBALT_FOR(auto pod, discover_pod)
                              {
                                  if (pod != nullptr) {
                                      pod_sessions.push_back(
                                          run_pod(std::move(pod), station_state.create_pod_state(), pod_registry));
                                  }
                              }

                              SPDLOG_LOGGER_WARN(logger, "Pod discovery finished.");
                          });
    SPDLOG_LOGGER_DEBUG(logger, "Pod manager exiting now.");
}
} // namespace cm
