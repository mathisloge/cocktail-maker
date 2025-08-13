// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/commands/dispense_liquid_cmd.hpp"
#include "cm/commands/command_visitor.hpp"
#include "cm/events/refill_ingredient_event.hpp"
#include "cm/execution_context.hpp"
#include "command_event_publisher.hpp"

namespace cm {
DispenseLiquidCmd::DispenseLiquidCmd(IngredientId ingredient, units::Litre volume, CommandId id)
    : Command{id}
    , ingredient_{std::move(ingredient)}
    , volume_{volume}
{
}

boost::asio::awaitable<void> DispenseLiquidCmd::run(ExecutionContext& ctx) const
{
    auto&& dispenser = ctx.liquid_registry().dispenser(ingredient_);

    auto remaining = volume_;
    CommandEventPublisher publisher{ctx.event_bus(), id()};
    while (remaining > 0 * mp_units::si::litre) {
        const auto available = dispenser.remaining_volume();
        const auto to_dispense = std::min(available, remaining);

        if (to_dispense > 0 * mp_units::si::litre) {
            co_await dispenser.dispense(to_dispense);
            remaining -= to_dispense;
        }

        if (remaining > 0 * mp_units::si::litre) {
            ctx.event_bus().publish(RefillIngredientEvent{.ingredient_id = ingredient_});
            co_await ctx.wait_for_resume();
        }
    }
}

void DispenseLiquidCmd::accept(CommandVisitor& visitor) const
{
    visitor.visit(*this);
}

const IngredientId& DispenseLiquidCmd::ingredient() const
{
    return ingredient_;
}

units::Litre DispenseLiquidCmd::volume() const
{
    return volume_;
}

} // namespace cm
