#include "cm/ingredient_store.hpp"
namespace cm
{

const Ingredient &IngredientStore::add_ingredient(Ingredient ingredient)
{
    auto &&[it, _] = ingredients_.emplace(ingredient.id, std::move(ingredient));
    return it->second;
}

const Ingredient &IngredientStore::find_ingredient(const IngredientId &id) const
{
    return ingredients_.find(id)->second;
}
} // namespace cm
