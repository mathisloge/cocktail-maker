module;
#include <boost/cobalt.hpp>

export module cm:pod_dispatcher;

import std;
import cm.core;
import :station_config;
import :pod_registry;
import :recipe;

namespace cobalt = boost::cobalt;

namespace cm {

export class PodDispatcher
{
  public:
    explicit PodDispatcher(PodRegistry& pod_registry, StationConfig& station_config)
        : pod_registry_{pod_registry}
        , station_config_{station_config}
    {
    }

    cobalt::promise<void> dispatch_dispense_command(DispenseCommand cmd)
    {
        const auto dispenser_assignment = station_config_.find_dispenser_for_ingredient(cmd.ingredient);
        if (not dispenser_assignment.has_value()) {
            throw std::move(dispenser_assignment.error());
        }
        auto dispenser = pod_registry_.dispenser_of_pod(dispenser_assignment->pod_id, dispenser_assignment->dispenser_id);
        if (not dispenser.has_value()) {
            throw std::move(dispenser.error());
        }
        co_await dispenser.value()->dispense(cmd.volume);
    }

  private:
    PodRegistry& pod_registry_;
    StationConfig& station_config_;
};

} // namespace cm
