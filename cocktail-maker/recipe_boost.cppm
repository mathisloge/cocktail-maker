module;
#include <libassert/assert-macros.hpp>

export module cm:recipe_boost;
import std;
import libassert;
import mp_units;
import cm.core;
import :ingredient;
import :recipe;

namespace cm {
namespace {

struct VolumeSummary
{
    units::Litre boostable_litres = 0.0 * units::milli_litre;
    units::Litre reducible_litres = 0.0 * units::milli_litre;
};

struct ScaleFactors
{
    double boostable = 1.0;
    double reducible = 1.0;
};

/// The boost category of the poured ingredient, or nothing for an ingredient the store doesn't know.
[[nodiscard]] auto boost_category_of(const DispenseCommand& pour, const IngredientStore& store) -> std::optional<BoostCategory>
{
    return store.find_by_id(pour.ingredient).transform([](const Ingredient& ingredient) { return ingredient.boost_category; });
}

[[nodiscard]] auto accumulate_volumes(VolumeSummary acc, const Command& cmd, const IngredientStore& store) -> VolumeSummary
{
    const auto* pour = std::get_if<DispenseCommand>(&cmd);
    if (pour == nullptr) {
        return acc;
    }
    // An unknown ingredient counts as fixed.
    switch (boost_category_of(*pour, store).value_or(BoostCategory::fixed)) {
    case BoostCategory::boostable:
        acc.boostable_litres += pour->volume;
        break;
    case BoostCategory::reducible:
        acc.reducible_litres += pour->volume;
        break;
    case BoostCategory::fixed:
        break;
    }
    return acc;
}

// Derives per-category scale factors from the volume split and boost ratio p ∈ [-1, 1].
//
// Positive p: shift (p × v_reducible) from reducible → boostable.
//   At p=+1 reducible drains to zero; boostable absorbs everything.
// Negative p: shift (|p| × v_boostable) from boostable → reducible.
//   At p=-1 boostable drains to zero; reducible absorbs everything.
//
// Total volume is conserved by construction.
[[nodiscard]] auto compute_scale_factors(const VolumeSummary& vol, double p) -> ScaleFactors
{
    const auto v_b = vol.boostable_litres.numerical_value_in(units::milli_litre);
    const auto v_r = vol.reducible_litres.numerical_value_in(units::milli_litre);

    if (p >= 0.0) {
        if (v_b <= 0.0) {
            return {}; // no boostable target — shift would vanish
        }
        return {
            .boostable = 1.0 + (p * v_r / v_b),
            .reducible = 1.0 - p,
        };
    }
    if (v_r <= 0.0) {
        return {}; // no reducible target — shift would vanish
    }
    return {
        .boostable = 1.0 + p,
        .reducible = 1.0 - (p * v_b / v_r),
    };
}

[[nodiscard]] auto scale_command(const Command& cmd, const ScaleFactors& scales, const IngredientStore& store) -> Command
{
    const auto* pour = std::get_if<DispenseCommand>(&cmd);
    if (pour == nullptr) {
        return cmd;
    }
    // An unknown ingredient passes through unchanged.
    switch (boost_category_of(*pour, store).value_or(BoostCategory::fixed)) {
    case BoostCategory::boostable:
        return DispenseCommand{pour->id, pour->ingredient, pour->volume * scales.boostable};
    case BoostCategory::reducible:
        return DispenseCommand{pour->id, pour->ingredient, pour->volume * scales.reducible};
    case BoostCategory::fixed:
        return cmd;
    }
    std::unreachable();
}

} // namespace

/**
 * @brief Redistributes volume between boostable and reducible ingredients.
 *
 * Fixed ingredients are untouched. The total dispensed volume is conserved:
 * whatever is added to boostable ingredients is subtracted from reducible
 * ones, and vice versa.
 *
 * @param commands   Recipe commands to adjust.
 * @param boost_pct  Strength in [-100 %, +100 %].
 *                   +100 % fully drains all reducible volume into boostable.
 *                   -100 % fully drains all boostable volume into reducible.
 * @param store      Ingredient store used to resolve boost categories.
 * @return A new Commands with scaled volumes; input is left unchanged.
 */
export [[nodiscard]] auto boost_recipe(const Commands& commands, units::Percent boost_pct, const IngredientStore& store)
    -> Commands
{
    ASSERT((units::abs(boost_pct) <= 100 * units::percent), "Boost can only be between -100% and 100%");
    const double p = boost_pct.numerical_value_in(units::percent) / 100.0; // normalise to [-1, 1]

    if (std::abs(p) < std::numeric_limits<double>::epsilon()) {
        return commands;
    }

    VolumeSummary volumes;
    for_each_command(commands, [&](const Command& cmd) { volumes = accumulate_volumes(volumes, cmd, store); });

    const auto scales = compute_scale_factors(volumes, p);

    return transform_commands(commands, [&](const Command& cmd) { return scale_command(cmd, scales, store); });
}

export [[nodiscard]] auto is_recipe_boostable(const Commands& commands, const IngredientStore& store) -> bool
{
    bool has_boostables = false;
    bool has_reducibles = false;
    for_each_command(commands, [&](const Command& cmd) {
        if (const auto* pour = std::get_if<DispenseCommand>(&cmd)) {
            const auto category = boost_category_of(*pour, store);
            has_boostables = has_boostables || category == BoostCategory::boostable;
            has_reducibles = has_reducibles || category == BoostCategory::reducible;
        }
    });
    return has_boostables && has_reducibles;
}
} // namespace cm
