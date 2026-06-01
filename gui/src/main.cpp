#include <boost/cobalt.hpp>
#include "app-window.h"

import std;
import mp_units;
import cm;
import cm.gui;

boost::cobalt::detached my_task(cm::Recipe r, std::shared_ptr<cm::BasicMachineAdapter> machine_adapter)
{
    co_await cm::process_commands(std::move(r.commands), std::move(machine_adapter));
}

int main(int argc, char** argv)
{
    const auto recipe_db_path = std::filesystem::path{"/Users/neleschoenrock/Desktop/MathisCode/cocktail-maker/db/recipes"};
    auto logger = cm::log::create_or_get("main");

    cm::log::info(*logger, "Setup application...");

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
    auto ui = cm::gui::AppWindow::create();
    boost::asio::io_context ctx;

    auto machine_adapter = std::make_shared<cm::gui::MachineAdapter>(ui, ingredient_store);

    ui->set_recipes(std::move(recipe_model));
    ui->on_create_recipe([&ctx, &recipe_store, machine_adapter](const cm::gui::RecipeView& recipe_to_create, int boost) {
        auto logger = cm::log::create_or_get("ui");
        cm::log::debug(logger, "create recipe '{}' with boost factor '{}'", recipe_to_create.name.begin(), boost);

        auto r = recipe_store.find_by_id(recipe_to_create.id).value();
        boost::asio::post(ctx, [recipe = std::move(r), machine_adapter]() { my_task(std::move(recipe), machine_adapter); });
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
