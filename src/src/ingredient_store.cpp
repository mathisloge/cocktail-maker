#include "cm/ingredient_store.hpp"
namespace cm
{

void IngredientStore::add_ingredient(Ingredient ingredient)
{
    ingredients_.emplace(ingredient.id, std::move(ingredient));
}

const Ingredient &IngredientStore::find_ingredient(const IngredientId &id) const
{
    return ingredients_.find(id)->second;
}
} // namespace cm
