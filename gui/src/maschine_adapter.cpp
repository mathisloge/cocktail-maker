module;
#include <boost/asio/any_io_executor.hpp>
#include <slint.h>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>
#include "app-window.h"

module cm.gui:machine_adapter_impl;
import std;
import cm.core;
import cm;
import :async_ui;
import :recipe_adapter;
import :machine_adapter;

namespace cm::gui {

Task<void> MachineAdapter::execute_command(ManualCommand command)
{
    auto logger = log::create_or_get("recipe");
    SPDLOG_LOGGER_DEBUG(logger, "Opening UI manual command popup for '{}'", command.instruction);

    auto wrapped_command = cm::Command{std::move(command)};
    auto ui_command = transform_command(wrapped_command, ingredient_store_);
    if (not ui_command.has_value()) {
        throw std::runtime_error("Command cannot be transformed into a ManualCommand");
    }

    co_await async_show_manual_command_popup(ui_, std::move(*ui_command), io_executor_);
    co_return;
}

Task<void> MachineAdapter::execute_command(DispenseCommand command)
{
    auto logger = log::create_or_get("recipe");
    SPDLOG_LOGGER_DEBUG(logger, "Process dispense command {}", command.ingredient);

    auto dispatcher = create_dispenser_for_ingredient(pod_registry_, station_config_, command.ingredient);

    constexpr auto kDispenseTolerance = 20 * units::milli_litre;

    units::Litre dispensed = 0 * units::milli_litre;
    bool needs_refill = false;

    try {
        dispensed = co_await dispatcher->dispense(command.volume);
        // If we missed the target by more than the tolerance, the pod is running low.
        needs_refill = units::abs(dispensed - command.volume) > kDispenseTolerance;
    }
    catch (const DispenserEmptyError&) {
        // Pod ran out mid-pour. Assume whatever was left (+-5ml sensor tolerance)
        // got dispensed before it failed.
        SPDLOG_LOGGER_DEBUG(logger, "Dispense failed: pod ran empty. Refilling and continuing...");
        dispensed = 0 * units::milli_litre;
        needs_refill = true;
    }

    if (needs_refill) {
        co_await execute_command(ManualCommand{.instruction = "Refill ingredient"});
        // After refill confirmation, assume the pod is full again.
        // Otherwise a new DispenserEmptyError will propagate and abort the cocktail.
        co_await dispatcher->dispense(command.volume - dispensed);
    }

    SPDLOG_LOGGER_DEBUG(logger, "Finished dispense command {}", command.ingredient);
}
} // namespace cm::gui
