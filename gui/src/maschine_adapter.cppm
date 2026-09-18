module;
#include <boost/asio/any_io_executor.hpp>
#include <slint.h>
#include <spdlog/spdlog.h>
#include "app-window.h"

export module cm.gui:machine_adapter;
import std;
import cm.core;
import cm;
import :async_ui;
import :recipe_adapter;

namespace cm::gui {

namespace asio = boost::asio;

export class MachineAdapter : public cm::BasicCommandExecuter
{
    slint::ComponentHandle<AppWindow> ui_;
    asio::any_io_executor io_executor_;
    const IngredientStore& ingredient_store_;
    const PodRegistry& pod_registry_;
    const StationConfig& station_config_;

  public:
    /// @param io_executor Executor of the io thread the commands are executed on.
    explicit MachineAdapter(slint::ComponentHandle<AppWindow> ui,
                            asio::any_io_executor io_executor,
                            const IngredientStore& ingredient_store,
                            const PodRegistry& pod_registry,
                            const StationConfig& station_config)
        : ui_{std::move(ui)}
        , io_executor_{std::move(io_executor)}
        , ingredient_store_{ingredient_store}
        , pod_registry_{pod_registry}
        , station_config_{station_config}
    {
    }

    Task<void> execute_command(ManualCommand command) override;

    Task<void> execute_command(DispenseCommand command) override;

    void update_command_status(cm::CommandId id, cm::CommandStatus status) override
    {
        auto logger = log::create_or_get("recipe");
        SPDLOG_LOGGER_DEBUG(logger, "Updating command '{}' status to '{}'", id, static_cast<int>(status));

        slint::invoke_from_event_loop([id, status, ui = ui_]() {
            auto selected = ui->global<RecipeContext>().get_active_recipe();
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
                    ui->global<RecipeContext>().set_active_recipe(selected);
                    return;
                }
            }
        });
    }
};
} // namespace cm::gui
