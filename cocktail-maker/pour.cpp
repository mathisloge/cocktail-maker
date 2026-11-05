module;
#include <spdlog/spdlog.h>

module cm:pour_impl;

import std;
import mp_units;
import cm.core;
import :pod;
import :pour;

namespace cm {

namespace {
Task<DispenseReport> dispense_or_empty(Dispenser& dispenser, const units::Litre volume)
{
    try {
        co_return co_await dispenser.dispense(volume);
    }
    catch (const DispenserEmptyError&) {
        co_return DispenseReport{.amount = 0.0 * units::milli_litre, .outcome = DispenseOutcome::bottle_empty};
    }
}
} // namespace

Task<void> pour_with_refill(Dispenser& dispenser,
                            const PodDispenser& address,
                            const units::Litre volume,
                            FillLevelTracker& fill_levels,
                            RefillPrompt prompt)
{
    auto logger = log::create_or_get("recipe");
    units::Litre remaining = volume;

    for (;;) {
        auto report = co_await dispense_or_empty(dispenser, remaining);
        // A full time limit with hardly anything poured points at a problem. Trims running out only means the pour
        // came close, unless less than half of a larger pour came out. Below that the load cell is too coarse to tell,
        // so only a pour with nothing at all counts, which also keeps the loop from running forever.
        const bool barely_moved_in_time_limit =
            report.outcome == DispenseOutcome::timed_out && report.amount < kMinTimedOutProgress;
        const bool far_short_after_trims =
            report.outcome == DispenseOutcome::still_short &&
            (remaining > kMinTimedOutProgress ? report.amount < remaining / 2 : report.amount <= 0.0 * units::milli_litre);
        if (barely_moved_in_time_limit || far_short_after_trims) {
            report.outcome = DispenseOutcome::bottle_empty;
        }
        co_await fill_levels.pour_finished(address, dispenser, report);

        if (report.outcome == DispenseOutcome::completed) {
            if (report.amount > remaining + kDispenseTolerance) {
                SPDLOG_LOGGER_WARN(logger, "Poured {} instead of {}, which can't be undone.", report.amount, remaining);
            }
            co_return;
        }

        remaining -= std::max<units::Litre>(report.amount, 0.0 * units::milli_litre);
        if (remaining <= kDispenseTolerance) {
            co_return;
        }
        if (report.outcome == DispenseOutcome::bottle_empty) {
            co_await prompt();
            co_await fill_levels.refilled(address, dispenser, RefillSource::mix_dialog);
        }
    }
}
} // namespace cm
