module;
#include <libassert/assert-macros.hpp>
#include <mp-units/math.h>
#include <mp-units/systems/si/units.h>

export module cm:recipe_boost;
import std;
import libassert;
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

// Generic left-fold over every Command leaf in a Commands tree.
template <typename T, typename Fn>
[[nodiscard]] auto fold_commands(const Commands& commands, T init, Fn fn) -> T
{
    const auto fold_cmd = [&](T acc, const Command& cmd) -> T { return fn(std::move(acc), cmd); };

    return std::accumulate(
        commands.cbegin(), commands.cend(), std::move(init), [&](T acc, const std::variant<Command, ParallelCommand>& item) -> T {
            return std::visit(
                [&](const auto& v) -> T {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<V, Command>) {
                        return fold_cmd(std::move(acc), v);
                    }
                    else { // ParallelCommand
                        return std::accumulate(v.cbegin(), v.cend(), std::move(acc), fold_cmd);
                    }
                },
                item);
        });
}

[[nodiscard]] auto accumulate_volumes(VolumeSummary acc, const Command& cmd, const IngredientStore& store) -> VolumeSummary
{
    return std::visit(
        [&](const auto& c) -> VolumeSummary {
            if constexpr (std::is_same_v<std::decay_t<decltype(c)>, DispenseCommand>) {
                return store.find_by_id(c.ingredient)
                    .transform([&](const Ingredient& ing) -> VolumeSummary {
                        switch (ing.boost_category) {
                        case BoostCategory::boostable:
                            return {acc.boostable_litres + c.volume, acc.reducible_litres};
                        case BoostCategory::reducible:
                            return {acc.boostable_litres, acc.reducible_litres + c.volume};
                        case BoostCategory::fixed:
                            return acc;
                        }
                        std::unreachable();
                    })
                    .value_or(acc);
            }
            return acc;
        },
        cmd);
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
    return std::visit(
        [&](const auto& c) -> Command {
            if constexpr (std::is_same_v<std::decay_t<decltype(c)>, DispenseCommand>) {
                return store.find_by_id(c.ingredient)
                    .transform([&](const Ingredient& ing) -> Command {
                        switch (ing.boost_category) {
                        case BoostCategory::boostable:
                            return DispenseCommand{c.id, c.ingredient, c.volume * scales.boostable};
                        case BoostCategory::reducible:
                            return DispenseCommand{c.id, c.ingredient, c.volume * scales.reducible};
                        case BoostCategory::fixed:
                            return cmd;
                        }
                        std::unreachable();
                    })
                    .value_or(cmd); // unknown ingredient → pass through unchanged
            }
            return cmd;
        },
        cmd);
}

[[nodiscard]] auto scale_commands(const Commands& commands, const ScaleFactors& scales, const IngredientStore& store) -> Commands
{
    const auto scale_item = [&](const std::variant<Command, ParallelCommand>& item) -> std::variant<Command, ParallelCommand> {
        return std::visit(
            [&](const auto& v) -> std::variant<Command, ParallelCommand> {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<V, Command>) {
                    return scale_command(v, scales, store);
                }
                else { // ParallelCommand
                    ParallelCommand result;
                    result.reserve(v.size());
                    std::ranges::transform(
                        v, std::back_inserter(result), [&](const Command& cmd) { return scale_command(cmd, scales, store); });
                    return result;
                }
            },
            item);
    };

    Commands result;
    result.reserve(commands.size());
    std::ranges::transform(commands, std::back_inserter(result), scale_item);
    return result;
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

    const auto volumes = fold_commands(commands, VolumeSummary{}, [&](VolumeSummary acc, const Command& cmd) {
        return accumulate_volumes(std::move(acc), cmd, store);
    });

    const auto scales = compute_scale_factors(volumes, p);

    return scale_commands(commands, scales, store);
}
} // namespace cm
