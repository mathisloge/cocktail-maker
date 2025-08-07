#include "cm/events/event_bus.hpp"
#include <spdlog/spdlog.h>
#include "cm/logging.hpp"

namespace cm
{
EventBus::EventBus()
{
    signal_.connect([](auto &&event) {
        static auto logger = LoggingContext::instance().create_logger("EventBus");
        SPDLOG_LOGGER_DEBUG(logger, "Event published: {}", event);
    });
}

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

auto fmt::formatter<cm::RecipeFinishedEvent>::format(const cm::RecipeFinishedEvent &e, format_context &ctx) const
    -> format_context::iterator
{
    return fmt::format_to(ctx.out(), "RecipeFinishedEvent");
}

auto fmt::formatter<cm::CommandStarted>::format(const cm::CommandStarted &e, format_context &ctx) const
    -> format_context::iterator
{
    return fmt::format_to(ctx.out(), "CommandStarted({})", e.cmd_id);
}

auto fmt::formatter<cm::CommandProgress>::format(const cm::CommandProgress &e, format_context &ctx) const
    -> format_context::iterator
{
    return fmt::format_to(ctx.out(), "CommandProgress({}, {})", e.cmd_id, e.progress);
}

auto fmt::formatter<cm::CommandFinished>::format(const cm::CommandFinished &e, format_context &ctx) const
    -> format_context::iterator
{
    return fmt::format_to(ctx.out(), "CommandFinished({})", e.cmd_id);
}

auto fmt::formatter<cm::Event>::format(const cm::Event &e, format_context &ctx) const -> format_context::iterator
{
    return std::visit([&ctx](const auto &v) { return fmt::format_to(ctx.out(), "Event({})", v); }, e);
}
