module;
export module cm.sim:simulated_pod_discovery;
import std;
import cm.core;
import cm;

namespace cm::sim {
/// Discovers two simulated pods, each connected to a simulated client through a local socket pair.
export class SimulatedPodDiscovery : public PodDiscovery
{
  public:
    Task<void> discover(AsyncScope& scope, std::function<void(std::shared_ptr<IPod>)> on_pod_discovered) override;
};
} // namespace cm::sim
