#pragma once
#include <mp-units/core.h>
#include <mp-units/systems/si/units.h>
namespace cm::units
{

using Grams = mp_units::quantity<mp_units::si::gram>;
using Litre = mp_units::quantity<mp_units::si::litre>;
inline constexpr decltype(mp_units::si::gram /
                          mp_units::si::litre) gram_per_litre; // NOLINT(readability-identifier-naming)
using GramPerLitre = mp_units::quantity<gram_per_litre>;

// vvv Stepper-Motor units
inline constexpr struct Step final : mp_units::named_unit<"steps", mp_units::one>
{
} step; // NOLINT(readability-identifier-naming)
using Steps = mp_units::quantity<step, std::int32_t>;
using StepsPerSecond = decltype((1 * step) / (1 * mp_units::si::second));
using StepsPerSecondSq = decltype(StepsPerSecond{} / mp_units::si::second);

inline constexpr decltype(step / mp_units::si::litre) steps_per_litre; // NOLINT(readability-identifier-naming)
using StepsPerLitre = mp_units::quantity<steps_per_litre>;
// ^^^ Stepper-Motor units

enum class OperationalState
{
    faulty,
    ok
};
} // namespace cm::units
