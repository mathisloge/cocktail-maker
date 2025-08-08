#pragma once
#include <memory>
#include <unordered_map>
#include "ingredient.hpp"
namespace cm
{
class IngredientStore : public std::enable_shared_from_this<IngredientStore>
{
  public:
    const Ingredient &add_ingredient(Ingredient ingredient);
    const Ingredient &find_ingredient(const IngredientId &id) const;

  private:
    std::unordered_map<IngredientId, Ingredient> ingredients_;
};
} // namespace cm
