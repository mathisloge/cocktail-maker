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
inline auto moscow_mule()
{
    // codespell:ignore-begin
    return make_recipe()
        .name(QT_TR_NOOP("Moscow Mule"))
        .description(QT_TR_NOOP("Der Moscow Mule zählt zu den beliebtesten Longdrinks weltweit. Hier treffen Ingweraromen auf "
                                "eine fruchtig-würzige Zitrusnote und eine angenehme Süße."))
        .image("qrc:/qt/qml/CocktailMaker/Db/assets/moscow_mule.png")
        .nominal_serving_volume(200 * units::milli_litre)
        .parallel_steps(std::make_unique<cm::DispenseLiquidCmd>(vodka, 40 * units::milli_litre, generate_unique_command_id()),
                        std::make_unique<cm::DispenseLiquidCmd>(lime_juice, 5 * units::milli_litre, generate_unique_command_id()))
        .step(std::make_unique<cm::DispenseLiquidCmd>(ginger_beer, 155 * units::milli_litre, generate_unique_command_id()))
        .step(std::make_unique<cm::ManualCmd>(QT_TR_NOOP("2 Eiswürfel"), generate_unique_command_id()))
        .create();
    // codespell:ignore-end
}
} // namespace cm::db
