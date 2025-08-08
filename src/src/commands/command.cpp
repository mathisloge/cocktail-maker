// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/commands/command.hpp"

namespace cm
{
Command::Command(CommandId command_id)
    : command_id_{command_id}
{}

CommandId Command::id() const
{
    return command_id_;
}
} // namespace cm
