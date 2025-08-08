// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <memory>
#include "cm/units.hpp"
namespace cm
{
class Recipe;
class IngredientStore;
class RecipeBooster
{
  public:
    [[nodiscard]] static bool is_boostable(const Recipe &recipe, const IngredientStore &store);
    [[nodiscard]] static std::shared_ptr<Recipe> boost_recipe(const Recipe &original,
                                                              const IngredientStore &store,
                                                              units::quantity<units::percent> boost);
};
} // namespace cm
