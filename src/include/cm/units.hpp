#pragma once
#include <mp-units/core.h>
#include <mp-units/systems/si/units.h>
namespace cm::units
{
using namespace mp_units;

inline constexpr struct percent final :// NOLINT(readability-identifier-naming)
    named_unit<"%", mag_ratio<1, 100> * one>
{
} percent; // NOLINT(readability-identifier-naming)

using Grams = quantity<si::gram>;
using Litre = quantity<si::litre>;
inline constexpr decltype(si::gram / si::litre) gram_per_litre; // NOLINT(readability-identifier-naming)
using GramPerLitre = quantity<gram_per_litre>;

// vvv Stepper-Motor units
inline constexpr struct Step final : named_unit<"steps", one>
{
} step; // NOLINT(readability-identifier-naming)
using Steps = quantity<step, std::int32_t>;
using StepsPerSecond = quantity<step / si::second>;
using StepsPerSecondSq = decltype(StepsPerSecond{} / si::second);

inline constexpr decltype(step / si::litre) steps_per_litre; // NOLINT(readability-identifier-naming)
using StepsPerLitre = quantity<steps_per_litre>;
// ^^^ Stepper-Motor units

enum class OperationalState
{
    faulty,
    ok
};
} // namespace cm::units
