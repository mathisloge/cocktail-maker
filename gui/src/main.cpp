#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt.hpp>
#include "app-window.h"

import std;
import mp_units;
import cm.core;
import cm;
import cm.gui;
import cm.sim;

boost::cobalt::detached my_task(cm::Recipe r, std::shared_ptr<cm::BasicCommandExecuter> command_executer)
{
    co_await cm::execute_commands(std::move(r.commands), std::move(command_executer));
}

boost::cobalt::detached my_task2(std::unique_ptr<cm::PodDiscovery> pod_discovery,
                                 cm::StationState& station_state,
                                 cm::PodRegistry& pod_registry)
{
    auto logger = cm::log::create_or_get("main");
    co_await cm::discover_and_run_pods(std::move(pod_discovery), station_state, pod_registry);
    cm::log::debug(logger, "simulated pod manager ended.");
}

int main(int argc, char** argv)
{
    boost::asio::io_context ctx;
    auto ui = cm::gui::AppWindow::create();

    auto ui_log_sink = std::make_shared<cm::gui::ui_log_sink_st>();
    cm::log::add_sink(ui_log_sink);

    const auto& ui_station_state_context = ui->global<cm::gui::StationStateContext>();
    ui_station_state_context.set_log_entries(ui_log_sink->model());

    auto logger = cm::log::create_or_get("main");
    cm::log::info(logger, "Setup application...");

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

    {
        const auto ingredients = ingredient_store.ingredients();
        std::vector<slint::SharedString> ingredient_ids;
        ingredient_ids.reserve(ingredients.size());
        std::ranges::transform(ingredients, std::back_inserter(ingredient_ids), [](const auto& ingredient) {
            return slint::SharedString(ingredient.raw().c_str());
        });
        ui->set_ingredient_ids(std::make_shared<slint::VectorModel<slint::SharedString>>(std::move(ingredient_ids)));
    }
    std::vector<cm::Recipe> recipes;
    for (int i = 0; i < 10; i++) {
        recipes.emplace_back(cm::Recipe{
            .display_name = "Mojito",
            .description = "Der Mojito ist ein erfrischender Cocktail aus Rum, Minze, Limette, Zucker und Soda – perfekt für "
                           "den Sommer.",
            .tags = {std::string{"classic"}},
            .image_path = recipe_db_path / "mojito.png",
            .commands =
                {
                    cm::DispenseCommand{.ingredient = cm::IngredientId{"test"}, .volume = (89 * cm::units::milli_litre)},
                    cm::ManualCommand{.instruction = "Help me"},
                    cm::ParallelCommand{
                        cm::DispenseCommand{.ingredient = cm::IngredientId{"test"}, .volume = (101 * cm::units::milli_litre)},
                        cm::DispenseCommand{.ingredient = cm::IngredientId{"test2"}, .volume = (101 * cm::units::milli_litre)},
                    },
                },
        });
    }
    cm::StationConfig station_config{ingredient_store};
    cm::PodRegistry pod_registry{};
    cm::PodDispatcher pod_dispatcher{pod_registry, station_config};
    cm::RecipeStore recipe_store{std::move(recipes)};
    cm::gui::DispenserCalibrationBridge dispenser_calibration_bridge{ctx.get_executor(), ui, pod_registry};
    auto command_executer = std::make_shared<cm::gui::MachineAdapter>(ui, ingredient_store, pod_registry, station_config);
    auto station_state = std::make_shared<cm::gui::StationStateBridge>(ui);

    ui->set_recipes(std::make_shared<cm::gui::RecipeModel>(recipe_store, ingredient_store));
    ui->on_create_recipe([&ctx, &recipe_store, command_executer](const cm::gui::RecipeView& recipe_to_create, int boost) {
        auto logger = cm::log::create_or_get("ui");
        cm::log::debug(logger, "create recipe '{}' with boost factor '{}'", recipe_to_create.name.begin(), boost);

        auto r = recipe_store.find_by_id(recipe_to_create.id).value();
        boost::asio::post(ctx.get_executor(),
                          [recipe = std::move(r), command_executer]() { my_task(std::move(recipe), command_executer); });
    });

    ui->on_boost_recipe([&](const int boost_percentage) {
        const auto boost = boost_percentage * cm::units::percent;
        auto logger = cm::log::create_or_get("ui");
        auto opt_recipe = recipe_store.find_by_id(ui->get_selected_recipe().id);
        if (not opt_recipe.has_value()) {
            cm::log::error(logger, "Could not find a recipe with id '{}'", ui->get_selected_recipe().id);
            return;
        }
        auto recipe = opt_recipe.value();
        cm::log::trace(logger, "boosting {} by {}.", recipe, boost);
        recipe.commands = cm::boost_recipe(recipe.commands, boost, ingredient_store);

        ui->set_selected_recipe(cm::gui::transform(recipe, ingredient_store));
    });

    ui->set_pods(station_state->pod_model());

    ui->on_assign_ingredient_to_dispenser([&](const cm::gui::Pod& pod,
                                              const cm::gui::Dispenser& dispenser,
                                              slint::SharedString ingredient_id) {
        auto logger = cm::log::create_or_get("ui");
        cm::log::trace(
            logger, "Assign ingredient '{}' to pod '{}' and dispenser '{}'", ingredient_id.data(), pod.id.data(), dispenser.id);
        boost::asio::post(ctx, [&station_config, pod, dispenser, ingredient_id]() {
            station_config.update_dispenser_ingredient_mapping(
                cm::IngredientId{ingredient_id.data()},
                {.pod_id = cm::PodId{pod.id.data()}, .dispenser_id = cm::DispenserId{dispenser.id}});
        });
    });

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

        cm::log::info(logger, "Async context finished.");
    });

    cm::log::info(*logger, "Run application...");
    ui->run();
    cm::log::info(*logger, "Application quit. Stopping async context...");
    ctx.stop();
    cobalt_thread.join();
    cm::log::info(*logger, "Async context joined. Finished.");
    return 0;
}
