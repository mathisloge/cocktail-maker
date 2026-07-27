module;
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

export class IngredientNotAssignedError : public std::runtime_error
{
  public:
    explicit IngredientNotAssignedError(IngredientId ingredient_id)
        : runtime_error{std::format("No dispenser is assigned to ingredient '{}'", ingredient_id)}
        , ingredient_id_{std::move(ingredient_id)}
    {
    }

    IngredientId ingredient_id() const
    {
        return ingredient_id_;
    }

  private:
    IngredientId ingredient_id_;
};

export class StationConfig final
{
  public:
    explicit StationConfig(const IngredientStore& ingredient_store, std::filesystem::path db_file);

    void init();

    void update_dispenser_ingredient_mapping(IngredientId ingredient_id, PodDispenser pod_dispenser_pair);

    std::expected<PodDispenser, IngredientNotAssignedError> find_dispenser_for_ingredient(IngredientId ingredient_id) const;

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
