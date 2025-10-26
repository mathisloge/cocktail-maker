// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "cm/events/event_bus.hpp"

namespace cm {
class CommandEventPublisher final
{
  public:
    CommandEventPublisher(EventBus& bus, CommandId command_id, std::optional<IngredientId> ingredient_id = std::nullopt);
    ~CommandEventPublisher();
    CommandEventPublisher(CommandEventPublisher&&) noexcept = delete;
    CommandEventPublisher(const CommandEventPublisher&) = delete;
    CommandEventPublisher& operator=(CommandEventPublisher&&) noexcept = delete;
    CommandEventPublisher& operator=(const CommandEventPublisher&) = delete;
    void progress(units::quantity<units::percent> percentage);

  private:
    EventBus& bus_;
    CommandId command_id_;
    std::optional<IngredientId> ingredient_id_;
};
} // namespace cm
