// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace cm {
enum class OperationState
{
    initializing,
    calibrating,
    faulty,
    ok
};
} // namespace cm
