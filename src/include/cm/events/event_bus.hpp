#pragma once
#include <variant>
#include <boost/signals2/signal.hpp>
#include <fmt/core.h>
#include "execution_canceled_event.hpp"
#include "manual_action_event.hpp"
#include "recipe_finished.hpp"
#include "refill_ingredient_event.hpp"

namespace cm
{
using Event = std::variant<ManualActionEvent, RefillIngredientEvent, ExecutionCanceledEvent, RecipeFinishedEvent>;
using EventSignal = boost::signals2::signal<void(const Event &)>;

class EventBus
{
  public:
    boost::signals2::connection subscribe(const EventSignal::slot_type &slot);
    void publish(const Event &e);

  private:
    EventSignal signal_;
};
} // namespace cm

template <>
struct fmt::formatter<cm::ManualActionEvent> : formatter<string_view>
{
    auto format(const cm::ManualActionEvent &e, format_context &ctx) const -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::RefillIngredientEvent> : formatter<string_view>
{
    auto format(const cm::RefillIngredientEvent &e, format_context &ctx) const -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::ExecutionCanceledEvent> : formatter<string_view>
{
    auto format(const cm::ExecutionCanceledEvent &e, format_context &ctx) const -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::RecipeFinishedEvent> : formatter<string_view>
{
    auto format(const cm::RecipeFinishedEvent &e, format_context &ctx) const -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::Event> : formatter<string_view>
{
    auto format(const cm::Event &e, format_context &ctx) const -> format_context::iterator;
};
