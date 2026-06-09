module;
#include <boost/cobalt.hpp>

export module cm:pod_discovery;

import std;
import :logging;
import :pod;
import :async_machine_interface;

namespace cm {

export class PodDiscovery
{
  public:
    virtual ~PodDiscovery() = default;
    virtual cobalt::generator<std::unique_ptr<BasicAsyncPodInterface>> discover() = 0;
};

cobalt::promise<void> run_pod(std::unique_ptr<BasicAsyncPodInterface> pod)
{
    auto logger{log::create_or_get("pod_discovery")};
    log::info(logger, "New pod discovered. Running it now...");
    co_await pod->run();
}

export cobalt::task<void> discover_and_run_pods(std::unique_ptr<PodDiscovery> pod_discovery)
{
    auto logger{log::create_or_get("pod_discovery")};
    co_await cobalt::with(
        cobalt::wait_group{},
        [logger, pod_discovery = std::move(pod_discovery)](cobalt::wait_group& pod_sessions) -> cobalt::promise<void> {
            auto discover_pod = pod_discovery->discover();
            BOOST_COBALT_FOR(auto pod, discover_pod)
            {
                if (pod != nullptr) {
                    pod_sessions.push_back(run_pod(std::move(pod)));
                }
            }

            log::warn(logger, "Pod discovery finished.");
        });
    log::debug(logger, "Pod manager exiting now.");
}
} // namespace cm
