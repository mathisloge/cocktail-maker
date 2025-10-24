// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <cm/execution_context.hpp>
#include <cm/ingredient_store.hpp>
#include <cm/recipe_store.hpp>

namespace cm::db {
// NOLINTBEGIN(readability-identifier-naming)
inline constexpr IngredientId water = "water";
inline constexpr IngredientId soda = "soda";
inline constexpr IngredientId vodka = "vodka";
inline constexpr IngredientId bacardi = "bacardi";
inline constexpr IngredientId lime_juice = "lime_juice";
inline constexpr IngredientId orange_juice = "orange_juice";

// NOLINTEND(readability-identifier-naming)

void register_ingredients(IngredientStore& ingredient_store);
void register_possible_recipes(const ExecutionContext& context, RecipeStore& recipe_store);
} // namespace cm::db
