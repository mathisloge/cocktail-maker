// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "ingredient_id.hpp"
#include "liquid_dispenser.hpp"

namespace cm {
class LiquidDispenserRegistry
{
  public:
    void register_dispenser(IngredientId ingredient, std::unique_ptr<LiquidDispenser> dispenser);
    LiquidDispenser& dispenser(const IngredientId& ingredient);

  private:
    std::unordered_map<IngredientId, std::unique_ptr<LiquidDispenser>> dispensers_;
};
} // namespace cm
