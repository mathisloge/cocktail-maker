#include "app-window.h"

import std;
import cm;
import cm.gui;

int main(int argc, char** argv)
{
    const auto recipe_db_path = std::filesystem::path{"/Users/neleschoenrock/Desktop/MathisCode/cocktail-maker/db/recipes"};
    auto logger = cm::log::create_or_get("main");

    cm::log::info(*logger, "test");

    cm::IngredientStore ingredient_store;

    auto recipe_model = std::make_shared<cm::gui::RecipeModel>(
        std::vector<cm::Recipe>{
            cm::Recipe{
                .display_name = "Mojito",
                .description = "Der Mojito ist ein erfrischender Cocktail aus Rum, Minze, Limette, Zucker und Soda – perfekt für "
                               "den Sommer.",
                .tags = {std::string{"classic"}},
                .image_path = recipe_db_path / "mojito.png",
            },
        },
        ingredient_store);
    auto ui = cm::gui::AppWindow::create();

    ui->set_recipes(std::move(recipe_model));

    ui->run();
    return 0;
}
