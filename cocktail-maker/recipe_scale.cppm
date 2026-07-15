module;
#include <libassert/assert-macros.hpp>

export module cm:recipe_scale;
import std;
import libassert;
import cm.core;
import :recipe;
import :ingredient;

namespace cm {
[[nodiscard]] auto scale_command_volume(const Command& cmd, const double factor) -> Command
{
    return std::visit(
        [&](const auto& c) -> Command {
            if constexpr (std::is_same_v<std::decay_t<decltype(c)>, DispenseCommand>) {
                return DispenseCommand{c.id, c.ingredient, c.volume * factor};
            }
            else { // ManualCommand, std::monostate
                return cmd;
            }
        },
        cmd);
}

[[nodiscard]] auto scale_commands_uniformly(const Commands& commands, const double factor) -> Commands
{
    const auto scale_item = [&](const std::variant<Command, ParallelCommand>& item) -> std::variant<Command, ParallelCommand> {
        return std::visit(
            [&](const auto& v) -> std::variant<Command, ParallelCommand> {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<V, Command>) {
                    return scale_command_volume(v, factor);
                }
                else { // ParallelCommand
                    ParallelCommand result;
                    result.reserve(v.size());
                    std::ranges::transform(
                        v, std::back_inserter(result), [&](const Command& cmd) { return scale_command_volume(cmd, factor); });
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

/**
 * @brief Uniformly scales every DispenseCommand volume in `commands` so the
 * total dispensed volume matches `target_volume`, proportional to
 * `nominal_serving_volume`.
 *
 * ManualCommand entries (and std::monostate) are left untouched.
 *
 * Unlike boost_recipe, this does not redistribute volume between ingredient
 * categories — every ingredient is scaled by the same factor
 * (target_volume / nominal_serving_volume). Because it is a uniform
 * multiply and boost_recipe conserves total volume by construction, the two
 * commute: scaling before or after boosting yields the same result.
 *
 * @param commands                Recipe commands to scale.
 * @param target_volume           Desired total serving volume, e.g. the
 *                                 selected glass's volume.
 * @param nominal_serving_volume  Recipe's reference volume
 *                                 (Recipe::nominal_serving_volume).
 * @return A new Commands with scaled volumes; input is left unchanged.
 */
export [[nodiscard]] auto scale_recipe(const Commands& commands,
                                       const units::Litre nominal_serving_volume,
                                       const units::Litre target_volume) -> Commands
{
    const auto nominal_ml = nominal_serving_volume.numerical_value_in(units::milli_litre);
    const auto target_ml = target_volume.numerical_value_in(units::milli_litre);

    ASSERT(nominal_ml > 0.0, "nominal_serving_volume must be positive");
    ASSERT(target_ml >= 0.0, "target_volume must be non-negative");

    const double factor = target_ml / nominal_ml;

    constexpr auto kEpsilon = std::numeric_limits<double>::epsilon() * 4.0;
    if (std::abs(factor - 1.0) <= kEpsilon) {
        return commands;
    }

    return scale_commands_uniformly(commands, factor);
}

} // namespace cm
