// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <string>
#include "cm/ingredient_id.hpp"

namespace cm
{

/**
 * @brief Categorizes how an ingredient should respond to a boost adjustment.
 *
 * Used by the boost feature to determine how an ingredient's quantity
 * changes when a recipe is positively or negatively boosted.
 */
enum class BoostCategory
{
    /**
     * @brief Quantity remains unchanged regardless of boost.
     *
     * Example: Fixed garnishes such as mint leaves or a slice of lemon.
     */
    fixed,
    /**
     * @brief Quantity increases when boost is positive, decreases when boost is negative.
     *
     * Example: Alcoholic ingredients such as rum or vodka.
     */
    boostable,
    /**
     * @brief Quantity decreases when boost is positive, increases when boost is negative.
     *
     * Example: Non-alcoholic ingredients such as juice or cola.
     */
    reducible,
};

struct Ingredient
{
    IngredientId id;
    std::string display_name;
    BoostCategory boost_category{};
};
} // namespace cm
