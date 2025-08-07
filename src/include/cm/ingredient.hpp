#pragma once
#include <string>
#include "cm/ingredient_id.hpp"

namespace cm
{
struct Ingredient
{
    IngredientId id;
    std::string display_name;
};
} // namespace cm
