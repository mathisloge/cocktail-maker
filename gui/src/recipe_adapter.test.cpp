#include <catch2/catch_test_macros.hpp>
#include <slint.h>
#include "app-window.h"

import std;
import mp_units;
import cm.core;
import cm;
import cm.gui;

using namespace cm::gui;
namespace units = cm::units;

namespace {

cm::Recipe make_recipe(std::string name, std::vector<std::string> tags, std::string description = "")
{
    if (description.empty()) {
        description = "desc_" + name;
    }
    return cm::Recipe{
        .id = cm::RecipeId{name},
        .display_name = std::move(name),
        .description = std::move(description),
        .tags = std::move(tags),
        .image_path = "",
    };
}

// Helper: extract tag strings from a RecipeView's tag_line model
std::vector<std::string> get_tags(const RecipeView& view)
{
    std::vector<std::string> result;
    const auto& model = view.tag_line;
    for (size_t i = 0; i < model->row_count(); ++i) {
        auto entry = model->row_data(i);
        if (entry) {
            result.emplace_back(entry->data());
        }
    }
    return result;
}

} // namespace

TEST_CASE("RecipeModel general conversion from std::vector<Recipe>", "[recipe_adapter][conversion]")
{
    cm::IngredientStore ingredient_store;
    SECTION("empty input produces empty model")
    {
        cm::RecipeStore recipe_store{};
        RecipeModel model(recipe_store, ingredient_store);
        CHECK(model.row_count() == 0);
    }

    SECTION("row_count matches number of recipes")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({
            make_recipe("A", {}),
            make_recipe("B", {}),
            make_recipe("C", {}),
        });
        RecipeModel model(recipe_store, ingredient_store);
        CHECK(model.row_count() == 3);
    }

    SECTION("id is preserved")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({
            make_recipe("Margherita Pizza", {}),
            make_recipe("Fungi Pizza", {}),
        });
        RecipeModel model(recipe_store, ingredient_store);

        auto maga_row = model.row_data(0);
        auto fungi_row = model.row_data(1);
        REQUIRE(maga_row.has_value());
        REQUIRE(fungi_row.has_value());
        CHECK(maga_row->id == "Margherita Pizza");
        CHECK(fungi_row->id == "Fungi Pizza");
    }

    SECTION("display_name is preserved as SharedString")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Margherita Pizza", {})});
        RecipeModel model(recipe_store, ingredient_store);

        auto row = model.row_data(0);
        REQUIRE(row.has_value());
        CHECK(std::string(row->name.data()) == "Margherita Pizza");
    }

    SECTION("description is preserved as SharedString")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Margherita Pizza", {}, "Just a basic pizza.")});
        RecipeModel model(recipe_store, ingredient_store);

        auto row = model.row_data(0);
        REQUIRE(row.has_value());
        CHECK(std::string(row->description.data()) == "Just a basic pizza.");
    }

    SECTION("out-of-bounds row_data returns nullopt")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Solo", {})});
        RecipeModel model(recipe_store, ingredient_store);
        CHECK_FALSE(model.row_data(1).has_value());
        CHECK_FALSE(model.row_data(99).has_value());
    }

    SECTION("row_data(0) on empty model returns nullopt")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({});
        RecipeModel model(recipe_store, ingredient_store);
        CHECK_FALSE(model.row_data(0).has_value());
    }

    SECTION("each recipe maps to its own row in order")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({
            make_recipe("First", {"a"}),
            make_recipe("Second", {"b"}),
            make_recipe("Third", {"c"}),
        });
        RecipeModel model(recipe_store, ingredient_store);

        CHECK(std::string(model.row_data(0)->name.data()) == "First");
        CHECK(std::string(model.row_data(1)->name.data()) == "Second");
        CHECK(std::string(model.row_data(2)->name.data()) == "Third");
    }

    SECTION("tag_line row_count matches original tags size")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Taggy", {"x", "y", "z"})});
        RecipeModel model(recipe_store, ingredient_store);

        auto row = model.row_data(0);
        REQUIRE(row.has_value());
        CHECK(row->tag_line->row_count() == 3);
    }

    SECTION("recipes with identical names are stored as distinct rows")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({
            make_recipe("Clone", {"original"}),
            make_recipe("Clone", {"copy"}),
        });
        RecipeModel model(recipe_store, ingredient_store);

        REQUIRE(model.row_count() == 2);
        CHECK(get_tags(*model.row_data(0))[0] == "ORIGINAL");
        CHECK(get_tags(*model.row_data(1))[0] == "COPY");
    }

    SECTION("image is loaded from path")
    {
        cm::Recipe r = make_recipe("Mojito", {});
        r.image_path = std::filesystem::path{TEST_IMAGE_DIR} / "mojito.png";

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto row = model.row_data(0);
        REQUIRE(row.has_value());

        const auto& img = row->image;
        CHECK(img.size().width == 400);
        CHECK(img.size().height == 400);
    }

    SECTION("empty image_path produces a null/empty image")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("No Image", {})});
        RecipeModel model(recipe_store, ingredient_store);

        auto row = model.row_data(0);
        REQUIRE(row.has_value());

        const auto& img = row->image;
        CHECK(img.size().width == 0);
        CHECK(img.size().height == 0);
    }
}

TEST_CASE("RecipeModel tag uppercasing", "[recipe_adapter][tags]")
{
    cm::IngredientStore ingredient_store;
    SECTION("lowercase tags are uppercased")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Pasta", {"vegan", "quick"})});
        RecipeModel model(recipe_store, ingredient_store);

        auto row = model.row_data(0);
        REQUIRE(row.has_value());

        auto tags = get_tags(*row);
        REQUIRE(tags.size() == 2);
        CHECK(tags[0] == "VEGAN");
        CHECK(tags[1] == "QUICK");
    }

    SECTION("already-uppercase tags are unchanged")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Steak", {"MEAT", "GRILL"})});
        RecipeModel model(recipe_store, ingredient_store);

        auto tags = get_tags(*model.row_data(0));
        CHECK(tags[0] == "MEAT");
        CHECK(tags[1] == "GRILL");
    }

    SECTION("mixed-case tags are fully uppercased")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Soup", {"GluTen-Free", "SpIcY"})});
        RecipeModel model(recipe_store, ingredient_store);

        auto tags = get_tags(*model.row_data(0));
        CHECK(tags[0] == "GLUTEN-FREE");
        CHECK(tags[1] == "SPICY");
    }

    SECTION("empty tag string remains empty")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Mystery", {""})});
        RecipeModel model(recipe_store, ingredient_store);

        auto tags = get_tags(*model.row_data(0));
        REQUIRE(tags.size() == 1);
        CHECK(tags[0] == "");
    }

    SECTION("recipe with no tags has empty tag_line model")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Plain", {})});
        RecipeModel model(recipe_store, ingredient_store);

        auto row = model.row_data(0);
        REQUIRE(row.has_value());
        CHECK(row->tag_line->row_count() == 0);
    }

    SECTION("tags are uppercased independently per recipe")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({
            make_recipe("A", {"vegan"}),
            make_recipe("B", {"meat"}),
        });
        RecipeModel model(recipe_store, ingredient_store);

        CHECK(get_tags(*model.row_data(0))[0] == "VEGAN");
        CHECK(get_tags(*model.row_data(1))[0] == "MEAT");
    }

    SECTION("numeric and symbol characters in tags are preserved")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Weird", {"top-5", "30min"})});
        RecipeModel model(recipe_store, ingredient_store);

        auto tags = get_tags(*model.row_data(0));
        CHECK(tags[0] == "TOP-5");
        CHECK(tags[1] == "30MIN");
    }
}

TEST_CASE("transform_command: ManualCommand", "[recipe_adapter][commands]")
{
    cm::IngredientStore ingredient_store;

    SECTION("manual command text is preserved")
    {
        cm::Recipe r = make_recipe("Test", {});
        r.commands = {cm::ManualCommand{.instruction = "Stir vigorously for 30 seconds"}};
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});

        RecipeModel model(recipe_store, ingredient_store);

        auto row = model.row_data(0);
        REQUIRE(row.has_value());
        REQUIRE(row->commands->row_count() == 1);

        auto cmd = row->commands->row_data(0);
        REQUIRE(cmd.has_value());
        CHECK(std::string(cmd->text.data()) == "Stir vigorously for 30 seconds");
        CHECK(cmd->status == CommandStatus::NotStarted);
    }

    SECTION("manual command with empty instruction is preserved")
    {
        cm::Recipe r = make_recipe("Test", {});
        r.commands = {cm::ManualCommand{.instruction = ""}};

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto cmd = model.row_data(0)->commands->row_data(0);
        REQUIRE(cmd.has_value());
        CHECK(std::string(cmd->text.data()) == "");
    }

    SECTION("multiple manual commands appear in order")
    {
        cm::Recipe r = make_recipe("Test", {});
        r.commands = {
            cm::ManualCommand{.instruction = "First"},
            cm::ManualCommand{.instruction = "Second"},
            cm::ManualCommand{.instruction = "Third"},
        };

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto commands = model.row_data(0)->commands;
        REQUIRE(commands->row_count() == 3);
        CHECK(std::string(commands->row_data(0)->text.data()) == "First");
        CHECK(std::string(commands->row_data(1)->text.data()) == "Second");
        CHECK(std::string(commands->row_data(2)->text.data()) == "Third");
    }

    SECTION("initial status is NotStarted")
    {
        cm::Recipe r = make_recipe("Test", {});
        r.commands = {cm::ManualCommand{.instruction = "Do something"}};

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto cmd = model.row_data(0)->commands->row_data(0);
        REQUIRE(cmd.has_value());
        CHECK(cmd->status == CommandStatus::NotStarted);
    }

    SECTION("command ids are copied")
    {
        cm::Recipe r = make_recipe("Test", {});
        r.commands = {cm::ManualCommand{.instruction = "Do something"}, cm::ManualCommand{.instruction = "Do nothing"}};

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto cmd0 = model.row_data(0)->commands->row_data(0);
        REQUIRE(cmd0.has_value());
        CHECK(cmd0->id == 1);

        auto cmd1 = model.row_data(0)->commands->row_data(1);
        REQUIRE(cmd1.has_value());
        CHECK(cmd1->id == 2);
    }
}

TEST_CASE("transform_command: DispenseCommand", "[recipe_adapter][commands]")
{
    SECTION("dispense command uses ingredient display name as text")
    {
        cm::IngredientStore ingredient_store;
        ingredient_store.init_ingredients({cm::Ingredient{.id = cm::IngredientId{"rum"}, .display_name = "Rum"}});

        cm::Recipe r = make_recipe("Mojito", {});
        r.commands = {cm::DispenseCommand{
            .ingredient = cm::IngredientId{"rum"},
            .volume = 50 * units::milli_litre,
        }};

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto cmd = model.row_data(0)->commands->row_data(0);
        REQUIRE(cmd.has_value());
        CHECK(std::string(cmd->text.data()) == "Rum");
    }

    SECTION("dispense command value contains volume in millilitres")
    {
        cm::IngredientStore ingredient_store;
        ingredient_store.init_ingredients({cm::Ingredient{.id = cm::IngredientId{"gin"}, .display_name = "Gin"}});

        cm::Recipe r = make_recipe("G&T", {});
        r.commands = {cm::DispenseCommand{
            .ingredient = cm::IngredientId{"gin"},
            .volume = 50 * units::milli_litre,
        }};

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto cmd = model.row_data(0)->commands->row_data(0);
        REQUIRE(cmd.has_value());
        CHECK(std::string(cmd->value.data()) == "50 mL");
    }

    SECTION("dispense command for unknown ingredient is skipped")
    {
        cm::IngredientStore ingredient_store; // empty — no ingredients registered

        cm::Recipe r = make_recipe("Ghost", {});
        r.commands = {cm::DispenseCommand{
            .ingredient = cm::IngredientId{"unknown"},
            .volume = 30 * units::milli_litre,
        }};
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});

        RecipeModel model(recipe_store, ingredient_store);

        // Unknown ingredient must be dropped, not crash or produce a null entry
        CHECK(model.row_data(0)->commands->row_count() == 0);
    }

    SECTION("dispense command volume is truncated to integer millilitres")
    {
        cm::IngredientStore ingredient_store;
        ingredient_store.init_ingredients({cm::Ingredient{.id = cm::IngredientId{"syrup"}, .display_name = "Syrup"}});

        cm::Recipe r = make_recipe("Fancy", {});
        r.commands = {cm::DispenseCommand{
            .ingredient = cm::IngredientId{"syrup"},
            .volume = 25 * units::milli_litre,
        }};

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto cmd = model.row_data(0)->commands->row_data(0);
        REQUIRE(cmd.has_value());
        CHECK(std::string(cmd->value.data()) == "25 mL");
    }

    SECTION("dispense command status is NotStarted")
    {
        cm::IngredientStore ingredient_store;
        ingredient_store.init_ingredients({cm::Ingredient{.id = cm::IngredientId{"vodka"}, .display_name = "Vodka"}});

        cm::Recipe r = make_recipe("Test", {});
        r.commands = {cm::DispenseCommand{
            .ingredient = cm::IngredientId{"vodka"},
            .volume = 40 * units::milli_litre,
        }};

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto cmd = model.row_data(0)->commands->row_data(0);
        REQUIRE(cmd.has_value());
        CHECK(cmd->status == CommandStatus::NotStarted);
    }

    SECTION("dispense command id is copied")
    {
        cm::IngredientStore ingredient_store;
        ingredient_store.init_ingredients({cm::Ingredient{.id = cm::IngredientId{"vodka"}, .display_name = "Vodka"}});

        cm::Recipe r = make_recipe("Test", {});
        r.commands = {cm::DispenseCommand{
                          .ingredient = cm::IngredientId{"vodka"},
                          .volume = 40 * units::milli_litre,
                      },
                      cm::DispenseCommand{
                          .ingredient = cm::IngredientId{"vodka"},
                          .volume = 50 * units::milli_litre,
                      }};

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        REQUIRE(model.row_data(0)->commands->row_count() == 2);

        auto cmd0 = model.row_data(0)->commands->row_data(0);
        REQUIRE(cmd0.has_value());
        CHECK(cmd0->id == 1);

        auto cmd1 = model.row_data(0)->commands->row_data(1);
        REQUIRE(cmd1.has_value());
        CHECK(cmd1->id == 2);
    }
}

TEST_CASE("transform: ParallelCommand flattening", "[recipe_adapter][commands]")
{
    SECTION("parallel commands are flattened into sequential entries")
    {
        cm::IngredientStore ingredient_store;
        ingredient_store.init_ingredients({
            cm::Ingredient{.id = cm::IngredientId{"a"}, .display_name = "Lemon Juice"},
            cm::Ingredient{.id = cm::IngredientId{"b"}, .display_name = "Sugar Syrup"},
        });

        cm::Recipe r = make_recipe("Lemonade", {});
        r.commands = {
            cm::ParallelCommand{
                cm::DispenseCommand{.ingredient = cm::IngredientId{"a"}, .volume = 30 * units::milli_litre},
                cm::DispenseCommand{.ingredient = cm::IngredientId{"b"}, .volume = 20 * units::milli_litre},
            },
        };

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto commands = model.row_data(0)->commands;
        REQUIRE(commands->row_count() == 2);
        CHECK(std::string(commands->row_data(0)->text.data()) == "Lemon Juice");
        CHECK(std::string(commands->row_data(1)->text.data()) == "Sugar Syrup");
    }

    SECTION("unknown ingredient inside parallel command is skipped, others remain")
    {
        cm::IngredientStore ingredient_store;
        ingredient_store.init_ingredients({cm::Ingredient{.id = cm::IngredientId{"a"}, .display_name = "Mint"}});

        cm::Recipe r = make_recipe("Mojito", {});
        r.commands = {
            cm::ParallelCommand{
                cm::DispenseCommand{.ingredient = cm::IngredientId{"unknown"}, .volume = 10 * units::milli_litre}, // unknown
                cm::DispenseCommand{.ingredient = cm::IngredientId{"a"}, .volume = 40 * units::milli_litre},
            },
        };

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto commands = model.row_data(0)->commands;
        REQUIRE(commands->row_count() == 1);
        CHECK(std::string(commands->row_data(0)->text.data()) == "Mint");
    }

    SECTION("mix of flat and parallel commands preserves overall order")
    {
        cm::IngredientStore ingredient_store;
        ingredient_store.init_ingredients({
            cm::Ingredient{.id = cm::IngredientId{"a"}, .display_name = "Tequila"},
            cm::Ingredient{.id = cm::IngredientId{"b"}, .display_name = "Triple Sec"},
        });

        cm::Recipe r = make_recipe("Margarita", {});
        r.commands = {
            cm::ManualCommand{.instruction = "Prepare glass"},
            cm::ParallelCommand{
                cm::DispenseCommand{.ingredient = cm::IngredientId{"a"}, .volume = 50 * units::milli_litre},
                cm::DispenseCommand{.ingredient = cm::IngredientId{"b"}, .volume = 25 * units::milli_litre},
            },
            cm::ManualCommand{.instruction = "Stir and serve"},
        };

        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({std::move(r)});
        RecipeModel model(recipe_store, ingredient_store);

        auto commands = model.row_data(0)->commands;
        REQUIRE(commands->row_count() == 4);
        CHECK(std::string(commands->row_data(0)->text.data()) == "Prepare glass");
        CHECK(std::string(commands->row_data(1)->text.data()) == "Tequila");
        CHECK(std::string(commands->row_data(2)->text.data()) == "Triple Sec");
        CHECK(std::string(commands->row_data(3)->text.data()) == "Stir and serve");
    }
}

TEST_CASE("transform: recipe with no commands", "[recipe_adapter][commands]")
{
    cm::IngredientStore ingredient_store;

    SECTION("empty command list produces empty command model")
    {
        cm::RecipeStore recipe_store;
        recipe_store.init_recipes({make_recipe("Empty", {})});
        RecipeModel model(recipe_store, ingredient_store);

        auto row = model.row_data(0);
        REQUIRE(row.has_value());
        CHECK(row->commands->row_count() == 0);
    }
}
