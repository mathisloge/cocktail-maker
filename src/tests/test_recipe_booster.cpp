// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>
#include "cm/commands/dispense_liquid_cmd.hpp"
#include "cm/ingredient.hpp"
#include "cm/ingredient_store.hpp"
#include "cm/recipe.hpp"
#include "cm/recipe_booster.hpp"

using namespace cm;

namespace {
consteval auto L(double litres) // NOLINT
{
    return litres * units::si::litre;
}
} // namespace

TEST_CASE("RecipeBooster preserves total variable volume when boosting", "[recipe_booster][volume]")
{
    auto builder = make_recipe().with_name("BoosterTest").with_nominal_serving_volume(L(0.25)).with_steps();

    builder.with_step(std::make_unique<DispenseLiquidCmd>("Vodka", L(0.04), generate_unique_command_id()));
    builder.with_step(std::make_unique<DispenseLiquidCmd>("Cola", L(0.10), generate_unique_command_id()));
    auto recipe = builder.add().create();

    IngredientStore store;
    store.add_ingredient(Ingredient{IngredientId{"Vodka"}, "Vodka", BoostCategory::boostable});
    store.add_ingredient(Ingredient{IngredientId{"Cola"}, "Cola", BoostCategory::reducible});

    units::Litre before_total = 0.0 * units::si::litre;
    for (const auto& step : recipe->production_steps()) {
        for (const auto& cmd : step) {
            if (auto&& d = dynamic_cast<const DispenseLiquidCmd*>(cmd.get())) {
                before_total += d->volume();
            }
        }
    }

    REQUIRE(before_total > 0.0 * units::si::litre);

    auto boosted = RecipeBooster::boost_recipe(*recipe, store, 50 * units::percent);

    units::Litre after_total = 0.0 * units::si::litre;
    for (const auto& step : boosted->production_steps()) {
        for (const auto& cmd : step) {
            if (auto&& d = dynamic_cast<const DispenseLiquidCmd*>(cmd.get())) {
                after_total += d->volume();
            }
        }
    }

    REQUIRE(after_total == before_total);
}

TEST_CASE("RecipeBooster increases boostable and reduces reducible on positive boost", "[recipe_booster][boost]")
{
    auto builder = make_recipe().with_name("BoosterTest2").with_nominal_serving_volume(L(0.25)).with_steps();

    builder.with_step(std::make_unique<DispenseLiquidCmd>("Vodka", L(0.04), generate_unique_command_id()));
    builder.with_step(std::make_unique<DispenseLiquidCmd>("Cola", L(0.10), generate_unique_command_id()));
    auto recipe = builder.add().create();

    IngredientStore store;
    store.add_ingredient(Ingredient{IngredientId{"Vodka"}, "Vodka", BoostCategory::boostable});
    store.add_ingredient(Ingredient{IngredientId{"Cola"}, "Cola", BoostCategory::reducible});

    // capture before volumes
    units::Litre vodka_before = 0.0 * units::si::litre;
    units::Litre cola_before = 0.0 * units::si::litre;
    for (const auto& step : recipe->production_steps()) {
        for (const auto& cmd : step) {
            if (auto&& d = dynamic_cast<const DispenseLiquidCmd*>(cmd.get())) {
                if (d->ingredient() == IngredientId{"Vodka"}) {
                    vodka_before += d->volume();
                }
                if (d->ingredient() == IngredientId{"Cola"}) {
                    cola_before += d->volume();
                }
            }
        }
    }

    auto boosted = RecipeBooster::boost_recipe(*recipe, store, 50 * units::percent);

    units::Litre vodka_after = 0.0 * units::si::litre;
    units::Litre cola_after = 0.0 * units::si::litre;
    for (const auto& step : boosted->production_steps()) {
        for (const auto& cmd : step) {
            if (auto&& d = dynamic_cast<const DispenseLiquidCmd*>(cmd.get())) {
                if (d->ingredient() == IngredientId{"Vodka"}) {
                    vodka_after += d->volume();
                }
                if (d->ingredient() == IngredientId{"Cola"}) {
                    cola_after += d->volume();
                }
            }
        }
    }

    REQUIRE(vodka_after > vodka_before);
    REQUIRE(cola_after < cola_before);
}

TEST_CASE("RecipeBooster handles negative boost (reduces boostables)", "[recipe_booster][reduce]")
{
    auto builder = make_recipe().with_name("BoosterTest3").with_nominal_serving_volume(L(0.25)).with_steps();

    builder.with_step(std::make_unique<DispenseLiquidCmd>("Vodka", L(0.04), generate_unique_command_id()));
    builder.with_step(std::make_unique<DispenseLiquidCmd>("Cola", L(0.10), generate_unique_command_id()));
    auto recipe = builder.add().create();

    IngredientStore store;
    store.add_ingredient(Ingredient{IngredientId{"Vodka"}, "Vodka", BoostCategory::boostable});
    store.add_ingredient(Ingredient{IngredientId{"Cola"}, "Cola", BoostCategory::reducible});

    // capture before volumes
    units::Litre vodka_before = 0.0 * units::si::litre;
    units::Litre cola_before = 0.0 * units::si::litre;
    for (const auto& step : recipe->production_steps()) {
        for (const auto& cmd : step) {
            if (auto&& d = dynamic_cast<const DispenseLiquidCmd*>(cmd.get())) {
                if (d->ingredient() == IngredientId{"Vodka"}) {
                    vodka_before += d->volume();
                }
                if (d->ingredient() == IngredientId{"Cola"}) {
                    cola_before += d->volume();
                }
            }
        }
    }

    auto boosted = RecipeBooster::boost_recipe(*recipe, store, -50 * units::percent);

    units::Litre vodka_after = 0.0 * units::si::litre;
    units::Litre cola_after = 0.0 * units::si::litre;
    for (const auto& step : boosted->production_steps()) {
        for (const auto& cmd : step) {
            if (auto&& d = dynamic_cast<const DispenseLiquidCmd*>(cmd.get())) {
                if (d->ingredient() == IngredientId{"Vodka"}) {
                    vodka_after += d->volume();
                }
                if (d->ingredient() == IngredientId{"Cola"}) {
                    cola_after += d->volume();
                }
            }
        }
    }

    REQUIRE(vodka_after < vodka_before);
    REQUIRE(cola_after >= cola_before);
}
