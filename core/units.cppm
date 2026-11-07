module;

export module cm.core:units;
import std;
import mp_units;

export namespace cm::units {
using namespace mp_units;

// NOLINTBEGIN(readability-identifier-naming)
using Grams = quantity<si::gram>;
using Litre = quantity<si::litre>;
using Percent = quantity<percent>;
inline constexpr auto gram_per_litre = si::gram / si::litre;
inline constexpr auto milli_litre = units::si::milli<units::si::litre>;
using GramPerLitre = quantity<gram_per_litre>;
using LitrePerSecond = quantity<si::litre / si::second>;

// NOLINTEND(readability-identifier-naming)
} // namespace cm::units
