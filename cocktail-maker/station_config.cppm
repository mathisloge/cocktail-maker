module;
#include <spdlog/spdlog.h>
export module cm:station_config;

import std;
import cm.core;
import :pod_types;
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
    explicit StationConfig(const IngredientStore& ingredient_store, std::filesystem::path db_file);

    void init();

    void update_dispenser_ingredient_mapping(IngredientId ingredient_id, PodDispenser pod_dispenser_pair);

    std::expected<PodDispenser, std::out_of_range> find_dispenser_for_ingredient(IngredientId ingredient_id) const;

    std::expected<IngredientId, std::out_of_range> find_ingredient_by_dispenser(PodDispenser pod_dispenser) const;

  private:
    static std::filesystem::path mapping_file_path();

    void save_config_to_file() const;

    void load_config_from_file();

  private:
    log::Logger logger_{log::create_or_get("station_config")};
    const IngredientStore& ingredient_store_;
    const std::filesystem::path db_file_;
    std::unordered_map<IngredientId, PodDispenser> ingredient_dispenser_mapping_;
};
} // namespace cm
