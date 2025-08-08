// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "cm/ingredient_id.hpp"
#include "cm/units.hpp"
#include "command.hpp"

namespace cm {
class DispenseLiquidCmd : public Command
{
  public:
    DispenseLiquidCmd(IngredientId ingredient, units::Litre volume, CommandId id);
    const IngredientId& ingredient() const;
    units::Litre volume() const;
    boost::asio::awaitable<void> run(ExecutionContext& ctx) const override;
    void accept(CommandVisitor& visitor) const override;

  private:
    IngredientId ingredient_;
    units::Litre volume_;
};
} // namespace cm
