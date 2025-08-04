#pragma once
#include "cm/ingredient_id.hpp"
#include "cm/units.hpp"
#include "command.hpp"

namespace cm
{
class DispenseLiquidCmd : public Command
{
  public:
    DispenseLiquidCmd(IngredientId ingredient, units::Litre volume);
    boost::asio::awaitable<void> run(ExecutionContext &ctx) const override;

  private:
    IngredientId ingredient_;
    units::Litre volume_;
};
} // namespace cm
