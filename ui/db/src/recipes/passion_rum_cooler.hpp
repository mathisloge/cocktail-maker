// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <cm/commands/dispense_liquid_cmd.hpp>
#include <cm/commands/manual_cmd.hpp>
#include <cm/recipe.hpp>
#include <qttranslation.h>
#include "cm/db/db.hpp"

namespace cm::db {
inline auto passion_rum_cooler()
{
    // codespell:ignore-begin
    return make_recipe()
        .name(QT_TR_NOOP("Passion Rum Cooler"))
        .description(QT_TR_NOOP("Der Passion Rum Cooler verbindet den tropischen Geschmack von Maracuja mit einem spritzigen "
                                "Limetouch und dem milden Charakter von Bacardi. Leicht, erfrischend und voller Urlaubsfeeling – "
                                "ein cooler Sommerdrink, der Lust auf mehr macht."))
        .image("qrc:/qt/qml/CocktailMaker/Db/assets/passion_rum_cooler.png")
        .nominal_serving_volume(250 * units::milli_litre)
        .parallel_steps(
            std::make_unique<cm::DispenseLiquidCmd>(bacardi, 40 * units::milli_litre, generate_unique_command_id()),
            std::make_unique<cm::DispenseLiquidCmd>(maracuja_juice, 110 * units::milli_litre, generate_unique_command_id()))
        .step(std::make_unique<cm::DispenseLiquidCmd>(lime_juice, 10 * units::milli_litre, generate_unique_command_id()))
        .step(std::make_unique<cm::DispenseLiquidCmd>(soda, 60 * units::milli_litre, generate_unique_command_id()))
        .step(std::make_unique<cm::ManualCmd>(QT_TR_NOOP("2 Eiswürfel"), generate_unique_command_id()))
        .create();
    // codespell:ignore-end
}
} // namespace cm::db
