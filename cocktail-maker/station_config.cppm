module;

export module cm:station_config;

import std;
import :logging;
import :pod;
import :ingredient;
import :dispenser;

namespace cm {

export struct PodDispenser
{
    PodId pod_id;
    DispenserId dispenser_id;

    friend constexpr auto operator<=>(const PodDispenser&, const PodDispenser&) = default;
};

export class StationConfig
{
  public:
    void update_dispenser_ingredient_mapping(IngredientId ingredient_id, PodDispenser pod_dispenser_pair)
    {
        log::debug(logger_,
                   "Pod '{}', Dispenser '{}' maps to ingredient '{}'",
                   pod_dispenser_pair.pod_id,
                   pod_dispenser_pair.dispenser_id,
                   ingredient_id);
        ingredient_dispenser_mapping_.insert_or_assign(std::move(ingredient_id), std::move(pod_dispenser_pair));
    }

    std::expected<PodDispenser, std::out_of_range> find_dispenser_for_ingredient(IngredientId ingredient_id) const
    {
        auto it = ingredient_dispenser_mapping_.find(ingredient_id);
        if (it == ingredient_dispenser_mapping_.end()) {
            return std::unexpected{
                std::out_of_range{std::format("Could not find a dispenser for ingredient '{}'.", ingredient_id)}};
        }
        return it->second;
    }

  private:
    log::Logger logger_{log::create_or_get("station_config")};
    std::unordered_map<IngredientId, PodDispenser> ingredient_dispenser_mapping_;
};
} // namespace cm
