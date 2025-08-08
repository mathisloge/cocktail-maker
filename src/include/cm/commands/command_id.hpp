// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <cstdint>
namespace cm
{
using CommandId = std::uint32_t;

CommandId generate_unique_command_id();
} // namespace cm
