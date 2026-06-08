module;
#include <boost/cobalt.hpp>

export module cm:pod_manager;

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

export class PodManager final
{
  public:
    PodManager(std::unique_ptr<PodDiscovery> pod_discovery)
        : logger_{log::create_or_get("pod")}
        , pod_discovery_{std::move(pod_discovery)}
    {
    }

    cobalt::task<void> run()
    {
        auto discover_pod = pod_discovery_->discover();
        co_await cobalt::with(cobalt::wait_group{}, [this](cobalt::wait_group& pod_sessions) -> cobalt::promise<void> {
            auto discover_pod = pod_discovery_->discover();
            BOOST_COBALT_FOR(auto pod, discover_pod)
            {
                if (pod != nullptr) {
                    pod_sessions.push_back(run_pod(std::move(pod)));
                }
            }
            log::warn(logger_, "Pod discovery finished.");
        });
        log::debug(logger_, "pod manager exiting now.");
    }

  private:
    cobalt::promise<void> run_pod(std::unique_ptr<BasicAsyncPodInterface> pod)
    {
        co_await pod->run();
    }

  private:
    log::Logger logger_;
    std::unique_ptr<PodDiscovery> pod_discovery_;
};
} // namespace cm
