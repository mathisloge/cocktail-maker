// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <boost/signals2/signal.hpp>
#include <fmt/core.h>
#include <variant>
#include "command_finished_event.hpp"
#include "command_progress_event.hpp"
#include "command_started_event.hpp"
#include "execution_canceled_event.hpp"
#include "manual_action_event.hpp"
#include "recipe_finished.hpp"
#include "refill_ingredient_event.hpp"

namespace cm {
using Event = std::variant<ManualActionEvent,
                           RefillIngredientEvent,
                           ExecutionCanceledEvent,
                           RecipeFinishedEvent,
                           CommandStarted,
                           CommandProgress,
                           CommandFinished>;
using EventSignal = boost::signals2::signal<void(const Event&)>;

class EventBus
{
  public:
    EventBus();
    boost::signals2::connection subscribe(const EventSignal::slot_type& slot);
    void publish(const Event& e);

  private:
    EventSignal signal_;
};
} // namespace cm

template <>
struct fmt::formatter<cm::ManualActionEvent> : formatter<string_view>
{
    auto format(const cm::ManualActionEvent& e, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::RefillIngredientEvent> : formatter<string_view>
{
    auto format(const cm::RefillIngredientEvent& e, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::ExecutionCanceledEvent> : formatter<string_view>
{
    auto format(const cm::ExecutionCanceledEvent& e, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::RecipeFinishedEvent> : formatter<string_view>
{
    auto format(const cm::RecipeFinishedEvent& e, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::CommandStarted> : formatter<string_view>
{
    auto format(const cm::CommandStarted& e, format_context& ctx) const -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::CommandProgress> : formatter<string_view>
{
    auto format(const cm::CommandProgress& e, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::CommandFinished> : formatter<string_view>
{
    auto format(const cm::CommandFinished& e, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct fmt::formatter<cm::Event> : formatter<string_view>
{
    auto format(const cm::Event& e, format_context& ctx) const -> format_context::iterator;
};
