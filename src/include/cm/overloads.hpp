// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
namespace cm
{
template <class... Ts>
struct overloads : Ts... // NOLINT
{
    using Ts::operator()...;
};
} // namespace cm
