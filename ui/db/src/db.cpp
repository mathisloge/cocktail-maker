// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/db/db.hpp"
#include <qttranslation.h>
#include "recipes/mojito.hpp"
#include "recipes/screwdriver.hpp"
#include "recipes/tequila_sunrise.hpp"

namespace cm::db {
void register_ingredients(IngredientStore& ingredient_store)
{
    ingredient_store.add_ingredient(Ingredient{
        .id = water,
        .display_name = QT_TR_NOOP("Wasser"),
        .boost_category = BoostCategory::reducible,
    });
    ingredient_store.add_ingredient(Ingredient{
        .id = bacardi,
        .display_name = QT_TR_NOOP("Bacardi"),
        .boost_category = BoostCategory::boostable,
    });
    ingredient_store.add_ingredient(Ingredient{
        .id = soda,
        .display_name = QT_TR_NOOP("Soda Wasser"),
        .boost_category = BoostCategory::reducible,
    });
    ingredient_store.add_ingredient(Ingredient{
        .id = lime_juice,
        .display_name = QT_TR_NOOP("Limettensaft"),
        .boost_category = BoostCategory::reducible,
    });
    ingredient_store.add_ingredient(Ingredient{
        .id = vodka,
        .display_name = QT_TR_NOOP("Vodka"),
        .boost_category = BoostCategory::boostable,
    });
    ingredient_store.add_ingredient(Ingredient{
        .id = orange_juice,
        .display_name = QT_TR_NOOP("Orangensaft"),
        .boost_category = BoostCategory::reducible,
    });
    ingredient_store.add_ingredient(Ingredient{
        .id = white_tequila,
        .display_name = QT_TR_NOOP("Tequila (weiß)"),
        .boost_category = BoostCategory::boostable,
    });
    ingredient_store.add_ingredient(Ingredient{
        .id = grenadine,
        .display_name = QT_TR_NOOP("Grenadine"),
        .boost_category = BoostCategory::reducible,
    });
}

void register_possible_recipes(const ExecutionContext& context, RecipeStore& recipe_store)
{
    recipe_store.add_recipe(mojito());
    recipe_store.add_recipe(screwdriver());
    recipe_store.add_recipe(tequila_sunrise());
}
} // namespace cm::db
