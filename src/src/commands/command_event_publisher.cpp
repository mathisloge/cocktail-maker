// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "command_event_publisher.hpp"

namespace cm {
CommandEventPublisher::CommandEventPublisher(EventBus& bus, CommandId command_id, std::optional<IngredientId> ingredient_id)
    : bus_{bus}
    , command_id_{command_id}
    , ingredient_id_{std::move(ingredient_id)}
{
    bus_.publish(CommandStarted{.cmd_id = command_id_, .ingredient_id = ingredient_id_});
}

CommandEventPublisher::~CommandEventPublisher()
{
    bus_.publish(CommandFinished{.cmd_id = command_id_, .ingredient_id = ingredient_id_});
}

void CommandEventPublisher::progress(units::quantity<units::percent> percentage)
{
    bus_.publish(CommandProgress{.cmd_id = command_id_, .progress = percentage});
}

} // namespace cm
