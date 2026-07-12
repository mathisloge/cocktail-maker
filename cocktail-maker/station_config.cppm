module;
#include <spdlog/spdlog.h>
export module cm:station_config;

import std;
import cm.core;
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

export class StationConfig final
{
  public:
    explicit StationConfig(const IngredientStore& ingredient_store, std::filesystem::path db_file)
        : ingredient_store_{ingredient_store}
        , db_file_{std::move(db_file)}
    {
    }

    void init()
    {
        // TODO: load from file
    }

    void update_dispenser_ingredient_mapping(IngredientId ingredient_id, PodDispenser pod_dispenser_pair)
    {
        SPDLOG_LOGGER_DEBUG(logger_,
                            "Pod '{}', Dispenser '{}' maps to ingredient '{}'",
                            pod_dispenser_pair.pod_id,
                            pod_dispenser_pair.dispenser_id,
                            ingredient_id);
        if (not ingredient_store_.find_by_id(ingredient_id).has_value()) {
            SPDLOG_LOGGER_ERROR(logger_, "Could not find ingredient '{}'.", ingredient_id);
            // question: throw exception here and bring it to the ui? Maybe with toasts?
            return;
        }
        // allow unknown pod->dispenser pair for now, as the config file will be loaded later on before anything is
        // discovered/connected.
        ingredient_dispenser_mapping_.insert_or_assign(std::move(ingredient_id), std::move(pod_dispenser_pair));
        save_config_to_file();
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

    std::expected<IngredientId, std::out_of_range> find_ingredient_by_dispenser(PodDispenser pod_dispenser) const
    {
        auto it =
            std::ranges::find_if(ingredient_dispenser_mapping_, [&](const auto& entry) { return entry.second == pod_dispenser; });

        if (it == ingredient_dispenser_mapping_.end()) {
            return std::unexpected(std::out_of_range("No ingredient found for the given pod/dispenser combination"));
        }

        return it->first;
    }

  private:
    void save_config_to_file() const
    {
    }

  private:
    log::Logger logger_{log::create_or_get("station_config")};
    const IngredientStore& ingredient_store_;
    const std::filesystem::path db_file_;
    std::unordered_map<IngredientId, PodDispenser> ingredient_dispenser_mapping_;
};
} // namespace cm
