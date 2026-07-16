module;
#include <libassert/assert-macros.hpp>
#include <simdjson/builder.h>
#include <simdjson/ondemand.h>
#include <simdjson/padded_string-inl.h>

module cm:ingredient_impl;
import std;
import mp_units;
import libassert;
import cm.core;
import :ingredient;

namespace cm {

void IngredientStore::init_ingredients(std::vector<Ingredient> ingredients)
{
    ingredients_.clear();
    for (auto&& i : ingredients) {
        const auto [_, emplaced] = ingredients_.emplace(i.id, std::move(i));
        ASSERT(emplaced);
    }
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
    std::vector<Ingredient> ingredients;
    simdjson::ondemand::parser parser;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            try {
                // Read via simdjson padded layout to support accelerated parsing
                simdjson::padded_string json = simdjson::padded_string::load(entry.path().c_str());
                simdjson::ondemand::document doc = parser.iterate(json);

                ingredients.push_back(parse_ingredient(doc));
            }
            catch (const simdjson::simdjson_error& e) {
                throw std::runtime_error(std::format("Failed to parse ingredient file {}: {}", entry.path().c_str(), e.what()));
            }
            catch (const std::exception& e) {
                throw std::runtime_error(std::format("Error in file {}: {}", entry.path().c_str(), e.what()));
            }
        }
    }
    return ingredients;
}
} // namespace cm