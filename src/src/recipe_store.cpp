// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/recipe_store.hpp"
#include "cm/recipe.hpp"

namespace cm {

RecipeStore::~RecipeStore() = default;

void RecipeStore::add_recipe(std::shared_ptr<Recipe> recipe)
{
    recipes_.emplace(recipe->name(), std::move(recipe));
}

const RecipeMap& RecipeStore::recipes() const
{
    return recipes_;
}

} // namespace cm
