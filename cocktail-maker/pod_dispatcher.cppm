module;
export module cm:pod_dispatcher;

import std;
import cm.core;
import :station_config;
import :pod_registry;
import :recipe;

namespace cm {

export std::unique_ptr<Dispenser> create_dispenser_for_ingredient(const PodRegistry& pod_registry,
                                                                  const StationConfig& station_config,
                                                                  IngredientId ingredient_id)
{
    const auto dispenser_assignment = station_config.find_dispenser_for_ingredient(ingredient_id);
    if (not dispenser_assignment.has_value()) {
        throw std::move(dispenser_assignment.error());
    }
    auto dispenser = pod_registry.dispenser_of_pod(dispenser_assignment->pod_id, dispenser_assignment->dispenser_id);
    if (not dispenser.has_value()) {
        throw std::move(dispenser.error());
    }
    return std::move(dispenser.value());
}
} // namespace cm
