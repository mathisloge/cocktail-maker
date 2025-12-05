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
inline auto virgin_sunrise()
{
    // codespell:ignore-begin
    return make_recipe()
        .name(QT_TR_NOOP("Virgin Sunrise"))
        .description(QT_TR_NOOP("Der Virgin Sunrise ist ein leuchtend bunter Mocktail aus Orangensaft, Maracuja und einem Hauch "
                                "Grenadine, der mit seinem sanften Farbverlauf sofort Sommerlaune aufkommen lässt. Fruchtig, süß "
                                "und vollkommen alkoholfrei – der perfekte Genuss für jede Gelegenheit."))
        .image("qrc:/qt/qml/CocktailMaker/Db/assets/virgin_sunrise.png")
        .nominal_serving_volume(250 * units::milli_litre)
        .parallel_steps(
            std::make_unique<cm::DispenseLiquidCmd>(orange_juice, 120 * units::milli_litre, generate_unique_command_id()),
            std::make_unique<cm::DispenseLiquidCmd>(maracuja_juice, 80 * units::milli_litre, generate_unique_command_id()))
        .step(std::make_unique<cm::DispenseLiquidCmd>(grenadine, 20 * units::milli_litre, generate_unique_command_id()))
        .step(std::make_unique<cm::ManualCmd>(QT_TR_NOOP("2 Eiswürfel"), generate_unique_command_id()))
        .create();
    // codespell:ignore-end
}
} // namespace cm::db
