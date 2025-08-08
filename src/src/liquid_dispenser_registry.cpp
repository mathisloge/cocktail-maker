// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/liquid_dispenser_registry.hpp"

namespace cm
{
void LiquidDispenserRegistry::register_dispenser(IngredientId ingredient, std::unique_ptr<LiquidDispenser> dispenser)
{
    dispensers_.insert_or_assign(std::move(ingredient), std::move(dispenser));
}

LiquidDispenser &LiquidDispenserRegistry::dispenser(const IngredientId &ingredient)
{
    return *dispensers_.at(ingredient);
}

} // namespace cm
