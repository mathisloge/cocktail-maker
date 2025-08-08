// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/ui/RecipeFactory.hpp"
#include "cm/ui/RecipeDetail.hpp"

namespace cm::ui {
RecipeFactory::RecipeFactory(std::shared_ptr<RecipeStore> recipe_store,
                             std::shared_ptr<const IngredientStore> ingredient_store)
    : recipe_store_{std::move(recipe_store)}
    , ingredient_store_{std::move(ingredient_store)}
{
}

RecipeDetail* RecipeFactory::create(const QString& recipeName)
{
    auto it = recipe_store_->recipes().find(recipeName.toStdString());
    if (it == recipe_store_->recipes().cend()) {
        return nullptr;
    }
    return new RecipeDetail{it->second,
                            ingredient_store_}; // NOLINT(cppcoreguidelines-owning-memory)
}
} // namespace cm::ui
