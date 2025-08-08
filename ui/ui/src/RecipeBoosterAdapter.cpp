// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RecipeBoosterAdapter.hpp"
#include <cm/recipe_booster.hpp>

namespace cm::ui
{
void RecipeBoosterAdapter::set_ingredient_store(cm::IngredientStore *ingredient_store)
{
    ingredient_store_ = ingredient_store->shared_from_this();
    Q_EMIT ingredient_store_changed();
}

cm::IngredientStore *RecipeBoosterAdapter::get_ingredient_store()
{
    return ingredient_store_.get();
}

bool RecipeBoosterAdapter::is_boostable() const
{
    if (original_recipe_ == nullptr or ingredient_store_ == nullptr)
    {
        return false;
    }
    return RecipeBooster::is_boostable(*original_recipe_->recipe(), *ingredient_store_);
}

cm::ui::RecipeDetail *RecipeBoosterAdapter::boost(std::int32_t percentage)
{
    auto boosted_recipe =
        RecipeBooster::boost_recipe(*original_recipe_->recipe(), *ingredient_store_, percentage * units::percent);
    return new RecipeDetail{std::move(boosted_recipe), ingredient_store_}; // NOLINT
}
} // namespace cm::ui
