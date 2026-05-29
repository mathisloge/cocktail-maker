module;

export module cm:units;
import std;
import mp_units;

export namespace cm::units {
using namespace mp_units;

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr struct percent final : named_unit<"%", mag_ratio<1, 100> * one>
{
} percent;

using Grams = quantity<si::gram>;
using Litre = quantity<si::litre>;
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
