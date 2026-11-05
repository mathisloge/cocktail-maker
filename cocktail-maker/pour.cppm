module;
export module cm:pour;

import std;
import mp_units;
import cm.core;
import :dispenser;
import :station_config;
import :fill_level;

namespace cm {

/// A pour that reached the Pod's time limit with less than this points at a problem. Above it a pour still short is judged by
/// the share that came out.
export inline constexpr units::Litre kMinTimedOutProgress = 20.0 * units::milli_litre;

/// Completes once somebody has confirmed the refill. A stop request completes it with `set_stopped`.
export using RefillPrompt = std::function<Task<void>()>;

/**
 * Pours `volume`, asks for a refill whenever the bottle runs empty, and pours the rest.
 *
 * The Pod stops a pour at its time limit whatever the amount, so that alone only leads to pouring the rest. An error of the
 * dispenser reaches the caller, while a failed weight read never stops a pour.
 */
export Task<void> pour_with_refill(
    Dispenser& dispenser, const PodDispenser& address, units::Litre volume, FillLevelTracker& fill_levels, RefillPrompt prompt);
} // namespace cm
