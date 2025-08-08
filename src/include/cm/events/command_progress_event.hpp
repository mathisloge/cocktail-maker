// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "cm/commands/command_id.hpp"
#include "cm/units.hpp"

namespace cm {
struct CommandProgress
{
    CommandId cmd_id;
    units::quantity<units::percent> progress;
};
} // namespace cm
