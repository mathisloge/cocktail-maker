#include "cm/events/event_bus.hpp"

namespace cm
{
boost::signals2::connection EventBus::subscribe(const EventSignal::slot_type &slot)
{
    return signal_.connect(slot);
}

void EventBus::publish(const Event &e)
{
    signal_(e);
}

} // namespace cm

auto fmt::formatter<cm::ManualActionEvent>::format(const cm::ManualActionEvent &e, format_context &ctx) const
    -> format_context::iterator
{
    return fmt::format_to(ctx.out(), "ManualActionEvent({})", e.instruction);
}

auto fmt::formatter<cm::RefillIngredientEvent>::format(const cm::RefillIngredientEvent &e, format_context &ctx) const
    -> format_context::iterator
{
    return fmt::format_to(ctx.out(), "RefillIngredient({})", e.ingredient_id);
}

auto fmt::formatter<cm::ExecutionCanceledEvent>::format(const cm::ExecutionCanceledEvent &e, format_context &ctx) const
    -> format_context::iterator
{
    return fmt::format_to(ctx.out(), "ExecutionCanceled");
}

auto fmt::formatter<cm::Event>::format(const cm::Event &e, format_context &ctx) const -> format_context::iterator
{
    return std::visit([&ctx](auto const &v) { return fmt::format_to(ctx.out(), "Event({})", v); }, e);
}
