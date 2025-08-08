// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <filesystem>
#include <gpiod.hpp>

namespace cm
{
template <typename T>
struct PinSelection
{
    std::filesystem::path chip;
    gpiod::line::offset offset;
};
} // namespace cm
