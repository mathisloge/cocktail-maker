#include "app-window.h"

import std;
import mp_units;
import cm;
import cm.gui;

int main(int argc, char** argv)
{
    const auto recipe_db_path = std::filesystem::path{"/Users/neleschoenrock/Desktop/MathisCode/cocktail-maker/db/recipes"};
    auto logger = cm::log::create_or_get("main");

    cm::log::info(*logger, "test");

    cm::IngredientStore ingredient_store;

    ingredient_store.add(cm::Ingredient{.id = "test",
                                        .display_name = "Test Ingredient",
                                        .type = cm::IngredientType::other,
                                        .boost_category = cm::BoostCategory::boostable});

    cm::RecipeStore recipe_store{{
        cm::Recipe{.display_name = "Mojito",
                   .description =
                       "Der Mojito ist ein erfrischender Cocktail aus Rum, Minze, Limette, Zucker und Soda – perfekt für "
                       "den Sommer.",
                   .tags = {std::string{"classic"}},
                   .image_path = recipe_db_path / "mojito.png",
                   .commands = {cm::DispenseCommand{.ingredient = "test", .volume = (89 * cm::units::milli_litre)}}},
    }};

    auto recipe_model = std::make_shared<cm::gui::RecipeModel>(recipe_store, ingredient_store);
    auto ui = cm::gui::AppWindow::create();

    ui->set_recipes(std::move(recipe_model));
    ui->on_create_recipe([](const cm::gui::RecipeView& recipe_to_create, int boost) {
        auto logger = cm::log::create_or_get("ui");
        cm::log::debug(logger, "create recipe '{}' with boost factor '{}'", recipe_to_create.name.begin(), boost);
    });

    ui->run();
    return 0;
}
