module;
#include <boost/cobalt.hpp>

export module cm:pod_discovery;

import std;
import :logging;
import :pod;
import :station_state;

namespace cm {

export class PodDiscovery
{
  public:
    virtual ~PodDiscovery() = default;
    virtual cobalt::generator<std::unique_ptr<IPod>> discover() = 0;
};

cobalt::promise<void> run_pod(std::unique_ptr<IPod> pod, std::shared_ptr<PodState> pod_state)
{
    auto logger{log::create_or_get("pod_discovery")};
    log::info(logger, "New pod discovered. Running it now...");
    co_await pod->run(std::move(pod_state));
}

export cobalt::task<void> discover_and_run_pods(std::unique_ptr<PodDiscovery> pod_discovery,
                                                std::shared_ptr<StationState> station_state)
{
    auto logger{log::create_or_get("pod_discovery")};
    co_await cobalt::with(cobalt::wait_group{},
                          [logger, pod_discovery = std::move(pod_discovery), station_state = std::move(station_state)](
                              cobalt::wait_group& pod_sessions) -> cobalt::promise<void> {
                              auto discover_pod = pod_discovery->discover();
                              BOOST_COBALT_FOR(auto pod, discover_pod)
                              {
                                  if (pod != nullptr) {
                                      pod_sessions.push_back(run_pod(std::move(pod), station_state->create_pod_state()));
                                  }
                              }

                              log::warn(logger, "Pod discovery finished.");
                          });
    log::debug(logger, "Pod manager exiting now.");
}
} // namespace cm
