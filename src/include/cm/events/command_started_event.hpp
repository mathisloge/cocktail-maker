// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <optional>
#include "cm/commands/command_id.hpp"
#include "cm/ingredient_id.hpp"

namespace cm {
struct CommandStarted
{
    CommandId cmd_id{};
    std::optional<IngredientId> ingredient_id;
};
} // namespace cm
