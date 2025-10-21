#pragma once
#include <cm/commands/dispense_liquid_cmd.hpp>
#include <cm/commands/manual_cmd.hpp>
#include <cm/recipe.hpp>
#include "cm/db/db.hpp"

namespace cm::db {
inline auto screwdriver()
{
    // codespell:ignore-begin
    return make_recipe()
        .name("Screwdriver")
        .description("Der Screwdriver ist ein erfrischender Cocktail aus Vodka und Orangensaft, der "
                     "durch seine einfache Zubereitung und fruchtige Note besticht.")
        .image("qrc:/qt/qml/CocktailMaker/Db/assets/screwdriver.png")
        .nominal_serving_volume(300 * units::milli_litre)
        .parallel_steps(
            std::make_unique<cm::DispenseLiquidCmd>(vodka, 100 * units::milli_litre, generate_unique_command_id()),
            std::make_unique<cm::DispenseLiquidCmd>(orange_juice, 150 * units::milli_litre, generate_unique_command_id()))
        .step(std::make_unique<cm::ManualCmd>("2 Eiswürfel", generate_unique_command_id()))
        .create();
    // codespell:ignore-end
}
} // namespace cm::db
