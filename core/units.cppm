module;
#include <mp-units/core.h>
#include <mp-units/systems/si/units.h>

export module cm.core:units;
import std;

export namespace cm::units {
using namespace mp_units;

// NOLINTBEGIN(readability-identifier-naming)
using Grams = quantity<si::gram>;
using Litre = quantity<si::litre>;
using Percent = quantity<percent>;
inline constexpr auto milli_litre = units::si::milli<units::si::litre>;

// vvv Stepper-Motor units
inline constexpr struct step final : named_unit<"steps", one>
{
} step;

using Steps = quantity<step, std::int32_t>;
// ^^^ Stepper-Motor units

// NOLINTEND(readability-identifier-naming)
} // namespace cm::units
