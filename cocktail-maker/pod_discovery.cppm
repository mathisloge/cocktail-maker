module;
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

    /**
     * Discovers pods until stopped (or until nothing more can be discovered).
     *
     * Every newly discovered pod is reported through `on_pod_discovered`, called on the io_context's thread. Background
     * work that belongs to discovery, but outlives a single pod report, is spawned into `scope`.
     */
    virtual Task<void> discover(AsyncScope& scope, std::function<void(std::shared_ptr<IPod>)> on_pod_discovered) = 0;
};

/// Runs `pod_discovery` and spawns a session for every discovered pod into `scope`, registered in `pod_registry` while it runs.
export Task<void> discover_and_run_pods(std::unique_ptr<PodDiscovery> pod_discovery,
                                        std::shared_ptr<StationState> station_state,
                                        PodRegistry& pod_registry,
                                        AsyncScope& scope);
} // namespace cm
