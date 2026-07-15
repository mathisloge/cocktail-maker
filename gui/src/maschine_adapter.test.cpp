#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/cobalt.hpp>
#include <catch2/catch_test_macros.hpp>
#include <slint.h>
#include "app-window.h"

import std;
import cm;
import cm.gui;

namespace {

inline void flush_slint_events()
{
    slint::invoke_from_event_loop([]() { slint::quit_event_loop(); });
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
}

} // namespace

TEST_CASE("MachineAdapter - manual command opens and resolves through the popup", "[gui][MachineAdapter]")
{
    auto ui = cm::gui::AppWindow::create();
    cm::IngredientStore ingredient_store;
    cm::PodRegistry pod_registry;
    cm::StationConfig station_config{ingredient_store, ""};

    cm::gui::MachineAdapter adapter{ui, ingredient_store, pod_registry, station_config};

    boost::asio::io_context ctx;
    bool completed = false;

    auto coro = [&]() -> boost::cobalt::task<void> {
        co_await adapter.execute_command(cm::ManualCommand{.instruction = "Prime pump"});
        completed = true;
    };

    boost::cobalt::spawn(ctx, coro(), boost::asio::detached);

    ctx.poll();
    flush_slint_events();

    ui->invoke_manual_command_confirmed();
    flush_slint_events();

    ctx.run();

    REQUIRE(completed);
}

TEST_CASE("MachineAdapter - update_command_status updates the active recipe", "[gui][MachineAdapter]")
{
    auto ui = cm::gui::AppWindow::create();
    cm::IngredientStore ingredient_store;
    cm::PodRegistry pod_registry;
    cm::StationConfig station_config{ingredient_store, ""};

    cm::gui::MachineAdapter adapter{ui, ingredient_store, pod_registry, station_config};

    auto active = ui->global<cm::gui::RecipeContext>().get_active_recipe();

    // Setup an initial mock command in the model with ID 42
    auto commands_model = std::make_shared<slint::VectorModel<cm::gui::Command>>();
    cm::gui::Command mock_cmd;
    mock_cmd.id = 42;
    mock_cmd.status = cm::gui::CommandStatus::NotStarted;
    commands_model->push_back(mock_cmd);

    // Assign to active recipe and set
    active.commands = commands_model;
    ui->global<cm::gui::RecipeContext>().set_active_recipe(active);
    flush_slint_events();

    // Trigger the update
    adapter.update_command_status(cm::CommandId{42}, cm::CommandStatus::in_progress);
    flush_slint_events();

    auto updated = ui->global<cm::gui::RecipeContext>().get_active_recipe();
    auto cmd = updated.commands->row_data(0);

    REQUIRE(cmd.has_value());
    REQUIRE(cmd->id == 42);
    REQUIRE(cmd->status == cm::gui::CommandStatus::InProgress);
}
