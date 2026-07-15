module;
#include <mp-units/bits/core_gmf.h>
export module cm.core:units;
import std;

// create our own c++ unit module as the mp-units module fails to compile with gcc-16
#define MP_UNITS_IN_MODULE_INTERFACE
#include <mp-units/core.h>
#include <mp-units/systems/si.h>

export namespace cm::units {
using namespace mp_units;

// NOLINTBEGIN(readability-identifier-naming)
using Grams = quantity<si::gram>;
using Litre = quantity<si::litre>;
using Percent = quantity<percent>;
inline constexpr auto gram_per_litre = si::gram / si::litre;
inline constexpr auto milli_litre = units::si::milli<units::si::litre>;
using GramPerLitre = quantity<gram_per_litre>;

// vvv Stepper-Motor units
inline constexpr struct step final : named_unit<"steps", one>
{
} step;

using Steps = quantity<step, std::int32_t>;
inline constexpr auto steps_per_litre = step / si::litre;
using StepsPerLitre = quantity<steps_per_litre>;
// ^^^ Stepper-Motor units

// NOLINTEND(readability-identifier-naming)
} // namespace cm::units
