// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <cm/commands/dispense_liquid_cmd.hpp>
#include <cm/commands/manual_cmd.hpp>
#include <cm/recipe.hpp>
#include "cm/db/db.hpp"

namespace cm::db {
inline auto mojito()
{
    // codespell:ignore-begin
    return make_recipe()
        .name("Mojito")
        .image("qrc:/qt/qml/CocktailMaker/Db/assets/mojito.png")
        .description("Der Mojito ist ein erfrischender Cocktail aus Rum, Minze, Limette, "
                     "Zucker und Soda – perfekt für den Sommer.")
        .nominal_serving_volume(250 * units::milli_litre)
        .parallel_steps(
            std::make_unique<cm::DispenseLiquidCmd>(bacardi, 89 * units::milli_litre, generate_unique_command_id()),
            std::make_unique<cm::DispenseLiquidCmd>(soda, 120 * units::milli_litre, generate_unique_command_id()),
            std::make_unique<cm::DispenseLiquidCmd>(lime_juice, 30 * units::milli_litre, generate_unique_command_id()))
        .step(std::make_unique<cm::ManualCmd>("2 Minzblätter", generate_unique_command_id()))
        .step(std::make_unique<cm::ManualCmd>("2 TL Zucker", generate_unique_command_id()))
        .create();
    // codespell:ignore-end
}
} // namespace cm::db
