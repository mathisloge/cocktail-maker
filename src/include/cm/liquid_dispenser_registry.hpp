// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <ranges>
#include "ingredient_id.hpp"
#include "liquid_dispenser.hpp"

namespace cm {
class LiquidDispenserRegistry
{
  public:
    void register_dispenser(IngredientId ingredient, std::unique_ptr<LiquidDispenser> dispenser);
    LiquidDispenser& dispenser(const IngredientId& ingredient);

    auto get_dispensers()
    {
        return std::ranges::ref_view(dispensers_) | std::views::values |
               std::views::transform([](auto& ptr) -> LiquidDispenser& { return *ptr; });
    }

  private:
    std::unordered_map<IngredientId, std::unique_ptr<LiquidDispenser>> dispensers_;
};
} // namespace cm
