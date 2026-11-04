module;
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

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
        if (report.outcome == DispenseOutcome::timed_out && report.amount < kMinTimedOutProgress) {
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
        if (report.outcome == DispenseOutcome::bottle_empty) {
            co_await prompt();
            co_await fill_levels.refilled(address, dispenser, RefillSource::mix_dialog);
        }
    }
}
} // namespace cm
