module;
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt.hpp>
#include <slint.h>
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
    PodDispatcher pod_dispatcher_;

  public:
    explicit MachineAdapter(slint::ComponentHandle<AppWindow> ui,
                            const IngredientStore& ingredient_store,
                            PodRegistry& pod_registry,
                            StationConfig& station_config)
        : ui_{std::make_shared<SlintAsyncAdapter>(std::move(ui), ingredient_store)}
        , pod_dispatcher_{pod_registry, station_config}
    {
    }

    cobalt::promise<void> execute_command(ManualCommand command) override
    {
        auto logger = log::create_or_get("recipe");
        log::debug(logger, "Opening UI manual command popup for '{}'", command.instruction);
        co_await async_show_manual_command_popup(ui_, std::move(command));
        co_return;
    }

    cobalt::promise<void> execute_command(DispenseCommand command) override
    {
        auto logger = log::create_or_get("recipe");

        log::debug{logger, "Process dispense command {}", command.ingredient};
        co_await pod_dispatcher_.dispatch_dispense_command(command);
        log::debug{logger, "Finished dispense command {}", command.ingredient};
        co_return;
    }

    void update_command_status(cm::CommandId id, cm::CommandStatus status) override
    {
        auto logger = log::create_or_get("recipe");
        log::debug(logger, "Updating command '{}' status to '{}'", id, static_cast<int>(status));

        ui_->run_in_ui_thread([id, status, ui = ui_]() {
            auto selected = ui->ui->get_selected_recipe();
            for (int i = 0; i < selected.commands->row_count(); i++) {
                auto cmd = selected.commands->row_data(i);
                if (cmd.has_value() and cmd->id == id) {
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
                    ui->ui->set_selected_recipe(selected);
                    return;
                }
            }
        });
    }
};
} // namespace cm::gui
