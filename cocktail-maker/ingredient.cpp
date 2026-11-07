module;
#include <simdjson.h>

module cm:ingredient_impl;
import std;
import mp_units;
import cm.core;
import :ingredient;
import :catalog_loader;

namespace cm {

void IngredientStore::init_ingredients(std::vector<Ingredient> ingredients)
{
    std::unordered_map<IngredientId, Ingredient> catalog;
    for (auto&& ingredient : ingredients) {
        auto id = ingredient.id;
        if (not catalog.emplace(id, std::move(ingredient)).second) {
            throw std::invalid_argument{std::format("Duplicate ingredient id '{}'", id)};
        }
    }
    ingredients_ = std::move(catalog);
}

std::optional<Ingredient> IngredientStore::find_by_id(const IngredientId& ingredient_id) const
{
    const auto it = ingredients_.find(ingredient_id);
    return (it != ingredients_.end()) ? std::optional{it->second} : std::nullopt;
}

std::vector<IngredientId> IngredientStore::ingredients() const
{
    std::vector<IngredientId> result;
    result.reserve(ingredients_.size());
    for (const auto& [id, _] : ingredients_) {
        result.emplace_back(id);
    }
    return result;
}

IngredientType parse_ingredient_type(std::string_view str)
{
    if (str == "alcohol")
        return IngredientType::alcohol;
    if (str == "juice")
        return IngredientType::juice;
    if (str == "syrup")
        return IngredientType::syrup;
    if (str == "soda")
        return IngredientType::soda;
    if (str == "water")
        return IngredientType::water;
    if (str == "dairy")
        return IngredientType::dairy;
    if (str == "bitters")
        return IngredientType::bitters;
    if (str == "puree")
        return IngredientType::puree;
    if (str == "other")
        return IngredientType::other;

    throw std::runtime_error(std::format("Unknown ingredient type: '{}'", str));
}

BoostCategory parse_boost_category(std::string_view str)
{
    if (str == "fixed")
        return BoostCategory::fixed;
    if (str == "boostable")
        return BoostCategory::boostable;
    if (str == "reducible")
        return BoostCategory::reducible;

    throw std::runtime_error(std::format("Unknown boost category: '{}'", str));
}

Ingredient parse_ingredient(simdjson::ondemand::document& doc)
{
    Ingredient ingredient;
    simdjson::ondemand::object obj = doc.get_object();

    ingredient.id = IngredientId{obj["id"].get_string().value()};
    ingredient.display_name = obj["display_name"].get_string().value();

    // 2. Parse Ingredient Type
    std::string_view type_str = obj["type"].get_string().value();
    ingredient.type = parse_ingredient_type(type_str);

    // 3. Parse Boost Category (JSON property key is "boost")
    std::string_view boost_str = obj["boost"].get_string().value();
    ingredient.boost_category = parse_boost_category(boost_str);

    return ingredient;
}

std::vector<Ingredient> load_ingredients_from_dir(std::filesystem::path dir)
{
    return load_json_catalog<Ingredient>(
        dir, "ingredient", [](simdjson::ondemand::document& document, const std::filesystem::path& /*file*/) {
            return parse_ingredient(document);
        });
}
} // namespace cm
