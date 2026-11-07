module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/system/error_code.hpp>
#include <slint.h>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>
#include "app-window.h"

module cm.gui;
import std;
import cm.core;
import cm;

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

    auto dispenser = create_dispenser_for_ingredient(pod_registry_, station_config_, command.ingredient);
    const auto address = station_config_.find_dispenser_for_ingredient(command.ingredient);
    if (not address.has_value()) {
        throw address.error();
    }

    co_await pour_with_refill(*dispenser, *address, command.volume, fill_levels_, [this, ingredient = command.ingredient]() {
        return ask_for_refill(ingredient);
    });

    SPDLOG_LOGGER_DEBUG(logger, "Finished dispense command {}", command.ingredient);
}

Task<void> MachineAdapter::ask_for_refill(IngredientId ingredient)
{
    co_await cm::async_receive(refill_prompt_token_);

    // It also has to come back when a stop request ends the prompt.
    struct TokenGuard
    {
        Channel<std::monostate>& channel;

        ~TokenGuard()
        {
            channel.try_send(boost::system::error_code{}, std::monostate{});
        }
    } token_guard{refill_prompt_token_};

    const auto found = ingredient_store_.find_by_id(ingredient);
    const auto name = found.has_value() ? found->display_name : ingredient.raw();
    co_await execute_command(ManualCommand{.instruction = std::format("{} nachfüllen. Bitte Personal rufen.", name)});
}
} // namespace cm::gui
