module;
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt.hpp>
#include <slint.h>
#include <spdlog/spdlog.h>
#include "app-window.h"

export module cm.gui:machine_adapter;
import std;
import cm;
import :async_ui;

namespace cm::gui {

namespace asio = boost::asio;
namespace cobalt = boost::cobalt;

export class MachineAdapter : public cm::BasicCommandExecuter
{
    std::shared_ptr<SlintAsyncAdapter> ui_;
    const PodRegistry& pod_registry_;
    const StationConfig& station_config_;

  public:
    explicit MachineAdapter(slint::ComponentHandle<AppWindow> ui,
                            const IngredientStore& ingredient_store,
                            const PodRegistry& pod_registry,
                            const StationConfig& station_config)
        : ui_{std::make_shared<SlintAsyncAdapter>(std::move(ui), ingredient_store)}
        , pod_registry_{pod_registry}
        , station_config_{station_config}
    {
    }

    cobalt::promise<void> execute_command(ManualCommand command) override
    {
        auto logger = log::create_or_get("recipe");
        SPDLOG_LOGGER_DEBUG(logger, "Opening UI manual command popup for '{}'", command.instruction);
        co_await async_show_manual_command_popup(ui_, std::move(command));
        co_return;
    }

    cobalt::promise<void> execute_command(DispenseCommand command) override
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

    void update_command_status(cm::CommandId id, cm::CommandStatus status) override
    {
        auto logger = log::create_or_get("recipe");
        SPDLOG_LOGGER_DEBUG(logger, "Updating command '{}' status to '{}'", id, static_cast<int>(status));

        ui_->run_in_ui_thread([id, status, ui = ui_]() {
            auto selected = ui->ui->global<RecipeContext>().get_active_recipe();
            for (int i = 0; i < selected.commands->row_count(); i++) {
                auto cmd = selected.commands->row_data(i);
                if (cmd.has_value() and cmd->id == id.raw()) {
                    const auto ui_status = [status]() {
                        switch (status) {
                        case unknown:
                            return CommandStatus::NotStarted;
                        case in_progress:
                            return CommandStatus::InProgress;
                        case finished:
                            return CommandStatus::Finished;
                        }
                        std::unreachable();
                    }();
                    cmd->status = ui_status;
                    selected.commands->set_row_data(i, *cmd);
                    ui->ui->global<RecipeContext>().set_active_recipe(selected);
                    return;
                }
            }
        });
    }
};
} // namespace cm::gui
