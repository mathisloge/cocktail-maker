#include <boost/cobalt.hpp>
#include <spdlog/spdlog.h>
#include "app-window.h"

import std;
import mp_units;
import cm.core;
import cm;
import cm.gui;
import cm.sim;

boost::cobalt::detached my_task2(std::unique_ptr<cm::PodDiscovery> pod_discovery,
                                 cm::StationState& station_state,
                                 cm::PodRegistry& pod_registry)
{
    auto logger = cm::log::create_or_get("main");
    co_await cm::discover_and_run_pods(std::move(pod_discovery), station_state, pod_registry);
    SPDLOG_LOGGER_DEBUG(logger, "simulated pod manager ended.");
}

int main(int argc, char** argv)
{
    boost::asio::io_context ctx;
    auto ui = cm::gui::AppWindow::create();

    auto ui_log_sink = std::make_shared<cm::gui::ui_log_sink_st>();
    cm::log::add_sink(ui_log_sink);

    cm::log::set_level(cm::log::Level::debug);

    const auto& ui_station_state_context = ui->global<cm::gui::StationStateContext>();
    ui_station_state_context.set_log_entries(ui_log_sink->model());

    auto logger = cm::log::create_or_get("main");
    SPDLOG_LOGGER_INFO(logger, "Setup application...");

    const auto recipe_db_path = std::filesystem::path{"/Users/neleschoenrock/Desktop/MathisCode/cocktail-maker/db/recipes"};

    cm::IngredientStore ingredient_store;

    ingredient_store.add(cm::Ingredient{.id = cm::IngredientId{"test"},
                                        .display_name = "Test Ingredient",
                                        .type = cm::IngredientType::other,
                                        .boost_category = cm::BoostCategory::boostable});
    ingredient_store.add(cm::Ingredient{.id = cm::IngredientId{"test2"},
                                        .display_name = "Test Ingredient2",
                                        .type = cm::IngredientType::other,
                                        .boost_category = cm::BoostCategory::reducible});

    std::vector<cm::Recipe> recipes;
    for (int i = 0; i < 10; i++) {
        recipes.emplace_back(cm::Recipe{
            .id = cm::RecipeId{std::format("mojito_{}", i)},
            .display_name = "Mojito",
            .description = "Der Mojito ist ein erfrischender Cocktail aus Rum, Minze, Limette, Zucker und Soda – perfekt für "
                           "den Sommer.",
            .tags = {std::string{"classic"}},
            .image_path = recipe_db_path / "mojito.png",
            .nominal_serving_volume = 250 * cm::units::milli_litre,
            .commands =
                {
                    cm::DispenseCommand{.ingredient = cm::IngredientId{"test"}, .volume = (75 * cm::units::milli_litre)},
                    cm::ManualCommand{.instruction = "Help me"},
                    cm::ParallelCommand{
                        cm::DispenseCommand{.ingredient = cm::IngredientId{"test"}, .volume = (100 * cm::units::milli_litre)},
                        cm::DispenseCommand{.ingredient = cm::IngredientId{"test2"}, .volume = (50 * cm::units::milli_litre)},
                    },
                },
        });
    }
    cm::StationConfig station_config{ingredient_store};
    cm::PodRegistry pod_registry{};
    cm::RecipeStore recipe_store{std::move(recipes)};
    cm::gui::DispenserCalibrationBridge dispenser_calibration_bridge{ctx.get_executor(), ui, pod_registry};
    cm::gui::RecipeContextBridge recipe_context_bridge{ctx.get_executor(), ui, recipe_store, ingredient_store, station_config};
    cm::gui::ProcessContextBridge process_context_bridge{
        ctx.get_executor(), ui, recipe_store, ingredient_store, station_config, pod_registry};
    cm::gui::GlassContextBridge glass_context_bridge{ui};
    auto station_state = std::make_shared<cm::gui::StationStateBridge>(ui, pod_registry, ctx.get_executor());

    auto work_guard = boost::asio::make_work_guard(ctx);
    std::thread cobalt_thread([&]() {
        auto logger = cm::log::create_or_get("cobalt_main");
        boost::cobalt::this_thread::set_executor(ctx.get_executor());

        cm::sim::Client sim_pod{cm::sim::Socket{ctx.get_executor()}, "Client1", {.major = 1}};
        boost::asio::post(ctx,
                          [station_state,
                           pod_discovery = std::make_unique<cm::sim::SimulatedPodDiscovery>(sim_pod),
                           &pod_registry]() mutable { my_task2(std::move(pod_discovery), *station_state, pod_registry); });

        ctx.run();

        SPDLOG_LOGGER_INFO(logger, "Async context finished.");
    });

    SPDLOG_LOGGER_INFO(logger, "Run application...");
    ui->run();
    SPDLOG_LOGGER_INFO(logger, "Application quit. Stopping async context...");
    ctx.stop();
    cobalt_thread.join();
    SPDLOG_LOGGER_INFO(logger, "Async context joined. Finished.");
    return 0;
}
