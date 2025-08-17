// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <filesystem>
#include "units.hpp"

namespace cm {
using GlassId = std::string;

struct Glass
{
    GlassId id;
    std::string display_name;
    std::filesystem::path image_path;
    units::Litre capacity;
};
} // namespace cm
