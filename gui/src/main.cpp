#include <boost/cobalt.hpp>
#include "app-window.h"

import std;
import mp_units;
import cm;
import cm.gui;

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

    cm::RecipeStore recipe_store{{
        cm::Recipe{.display_name = "Mojito",
                   .description =
                       "Der Mojito ist ein erfrischender Cocktail aus Rum, Minze, Limette, Zucker und Soda – perfekt für "
                       "den Sommer.",
                   .tags = {std::string{"classic"}},
                   .image_path = recipe_db_path / "mojito.png",
                   .commands =
                       {
                           cm::DispenseCommand{.ingredient = "test", .volume = (89 * cm::units::milli_litre)},
                           cm::DispenseCommand{.ingredient = "test2", .volume = (101 * cm::units::milli_litre)},
                       }},
    }};

    auto recipe_model = std::make_shared<cm::gui::RecipeModel>(recipe_store, ingredient_store);
    auto ui = cm::gui::AppWindow::create();

    ui->set_recipes(std::move(recipe_model));
    ui->on_create_recipe([](const cm::gui::RecipeView& recipe_to_create, int boost) {
        auto logger = cm::log::create_or_get("ui");
        cm::log::debug(logger, "create recipe '{}' with boost factor '{}'", recipe_to_create.name.begin(), boost);
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

    boost::asio::io_context ctx;
    std::thread cobalt_thread([&ctx]() {
        boost::cobalt::this_thread::set_executor(ctx.get_executor());

        ctx.run();
    });

    cm::log::info(*logger, "Run application...");
    ui->run();
    cm::log::info(*logger, "Application quit. Stopping async context...");
    ctx.stop();
    cobalt_thread.join();
    cm::log::info(*logger, "Async context joined. Finished.");
    return 0;
}
