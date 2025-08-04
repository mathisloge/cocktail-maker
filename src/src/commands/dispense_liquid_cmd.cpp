#include "cm/commands/dispense_liquid_cmd.hpp"
#include "cm/events/refill_ingredient_event.hpp"
#include "cm/execution_context.hpp"

namespace cm
{
DispenseLiquidCmd::DispenseLiquidCmd(IngredientId ingredient, units::Litre volume)
    : ingredient_{std::move(ingredient)}
    , volume_{volume}
{}

boost::asio::awaitable<void> DispenseLiquidCmd::run(ExecutionContext &ctx) const
{
    auto &&dispenser = ctx.liquid_registry().dispenser(ingredient_);

    if (volume_ > dispenser.remaining_volume())
    {
        auto remaining = volume_ - dispenser.remaining_volume();
        co_await dispenser.dispense(dispenser.remaining_volume());
        ctx.event_bus().publish(RefillIngredientEvent{.ingredient_id = ingredient_});
        co_await ctx.wait_for_resume();
        co_await dispenser.dispense(remaining);
    }
    else
    {
        co_await dispenser.dispense(volume_);
    }
}
} // namespace cm
