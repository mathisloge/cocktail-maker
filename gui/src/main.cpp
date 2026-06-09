#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt.hpp>
#include "app-window.h"

import std;
import mp_units;
import cm;
import cm.gui;
import cm.sim;

boost::cobalt::detached my_task(cm::Recipe r, std::shared_ptr<cm::BasicCommandExecuter> command_executer)
{
    co_await cm::execute_commands(std::move(r.commands), std::move(command_executer));
}

boost::cobalt::detached my_task2(std::unique_ptr<cm::PodDiscovery> pod_discovery)
{
    auto logger = cm::log::create_or_get("main");
    co_await cm::discover_and_run_pods(std::move(pod_discovery));
    cm::log::debug(logger, "simulated pod manager ended.");
}

int main(int argc, char** argv)
{
    boost::asio::io_context ctx;
    auto ui = cm::gui::AppWindow::create();

    auto ui_log_sink = std::make_shared<cm::gui::ui_log_sink_st>();
    cm::log::add_sink(ui_log_sink);
    ui->set_log_entries(ui_log_sink->model());

    auto logger = cm::log::create_or_get("main");
    cm::log::info(logger, "Setup application...");

    const auto recipe_db_path = std::filesystem::path{"/Users/neleschoenrock/Desktop/MathisCode/cocktail-maker/db/recipes"};

    cm::IngredientStore ingredient_store;

    ingredient_store.add(cm::Ingredient{.id = "test",
                                        .display_name = "Test Ingredient",
                                        .type = cm::IngredientType::other,
                                        .boost_category = cm::BoostCategory::boostable});
    ingredient_store.add(cm::Ingredient{.id = "test2",
                                        .display_name = "Test Ingredient2",
                                        .type = cm::IngredientType::other,
                                        .boost_category = cm::BoostCategory::reducible});

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
                    cm::DispenseCommand{.ingredient = "test", .volume = (89 * cm::units::milli_litre)},
                    cm::ManualCommand{.instruction = "Help me"},
                    cm::ParallelCommand{
                        cm::DispenseCommand{.ingredient = "test", .volume = (101 * cm::units::milli_litre)},
                        cm::DispenseCommand{.ingredient = "test2", .volume = (101 * cm::units::milli_litre)},
                    },
                },
        });
    }
    cm::RecipeStore recipe_store{std::move(recipes)};
    auto recipe_model = std::make_shared<cm::gui::RecipeModel>(recipe_store, ingredient_store);

    auto command_executer = std::make_shared<cm::gui::MachineAdapter>(ui, ingredient_store);

    ui->set_recipes(std::move(recipe_model));
    ui->on_create_recipe([&ctx, &recipe_store, command_executer](const cm::gui::RecipeView& recipe_to_create, int boost) {
        auto logger = cm::log::create_or_get("ui");
        cm::log::debug(logger, "create recipe '{}' with boost factor '{}'", recipe_to_create.name.begin(), boost);

        auto r = recipe_store.find_by_id(recipe_to_create.id).value();
        boost::asio::post(ctx, [recipe = std::move(r), command_executer]() { my_task(std::move(recipe), command_executer); });
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

    auto work_guard = boost::asio::make_work_guard(ctx);
    std::thread cobalt_thread([&ctx, recipe = recipe_store.find_by_id(0)]() {
        auto logger = cm::log::create_or_get("cobalt_main");
        boost::cobalt::this_thread::set_executor(ctx.get_executor());

        boost::asio::post(ctx, [pod_manager = std::make_unique<cm::sim::SimulatedPodDiscovery>(ctx.get_executor())]() mutable {
            my_task2(std::move(pod_manager));
        });

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
