module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/cobalt/detached.hpp>
#include <slint.h>
#include "app-window.h"

export module cm.gui:process_context_bridge;

import std;
import mp_units;
import cm.core;
import cm;
import :recipe_adapter;
import :machine_adapter;

namespace cm::gui {

export class ProcessContextBridge
{
  public:
    explicit ProcessContextBridge(boost::asio::any_io_executor executor,
                                  slint::ComponentHandle<AppWindow> ui,
                                  RecipeStore& recipe_store,
                                  IngredientStore& ingredient_store,
                                  cm::StationConfig& station_config,
                                  PodRegistry& pod_registry)
        : executor_{std::move(executor)}
        , ui_{std::move(ui)}
        , recipe_store_{recipe_store}
        , ingredient_store_{ingredient_store}
        , station_config_{station_config}
        , pod_registry_{pod_registry}
    {
        ui_->global<ProcessContext>().on_process_active_recipe([this](const int boost_raw) {
            const auto recipe_to_create = ui_->global<RecipeContext>().get_active_recipe();
            const auto boost = boost_raw * units::percent;
            auto r = recipe_store_.find_by_id(RecipeId{recipe_to_create.id.data()});
            if (r.has_value()) {
                cm::log::debug(logger_, "create {} with boost factor '{}'", r.value(), boost);
                r->commands = boost_recipe(r->commands, boost, ingredient_store_);
                boost::asio::post(executor_, [recipe = std::move(r.value()), this]() { async_process_recipe(recipe); });
            }
            else {
                log::error(logger_, "Could not find a recipe {}", recipe_to_create.name.data());
            }
        });
    }

  private:
    boost::cobalt::detached async_process_recipe(Recipe recipe)
    {
        auto command_executer = std::make_shared<MachineAdapter>(ui_, ingredient_store_, pod_registry_, station_config_);
        try {
            co_await cm::execute_commands(std::move(recipe.commands), std::move(command_executer));
        }
        catch (const std::exception& ex) {
            log::error(logger_, "Unknown error while processing recipe: {}", ex.what());
            display_ui_error(ex);
        }
    }

    void display_ui_error(const std::exception& ex)
    {
        auto error_desc = slint::SharedString{ex.what()};
        slint::invoke_from_event_loop([ui = ui_, error_desc = std::move(error_desc)]() {
            const auto& process_ctx = ui->global<ProcessContext>();
            process_ctx.set_error_title("Unbekannter Fehler");
            process_ctx.set_error_description(std::move(error_desc));
            process_ctx.set_error_item_category("U");
            process_ctx.set_error_item_name("Unbekannt");
            process_ctx.set_error_item_details("");
            ui->global<StationStateContext>().set_active_screen(Page::MixErrorPage);
        });
    }

  private:
    log::Logger logger_{log::create_or_get("ui")};
    boost::asio::any_io_executor executor_;
    slint::ComponentHandle<AppWindow> ui_;
    PodRegistry& pod_registry_;
    RecipeStore& recipe_store_;
    IngredientStore& ingredient_store_;
    cm::StationConfig& station_config_;
};
} // namespace cm::gui
