// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "cm/ingredient_id.hpp"

namespace cm {
struct RefillIngredientEvent
{
    IngredientId ingredient_id;
};
} // namespace cm
