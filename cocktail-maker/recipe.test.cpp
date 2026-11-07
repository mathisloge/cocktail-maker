#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
import cm;
import cm.core;
import mp_units;
import cm.testing;
import std;
using namespace cm;

namespace {
Recipe make_recipe(std::string name, std::string description = "", int n_commands = 0)
{
    Commands commands;
    for (int i = 0; i < n_commands; i++) {
        commands.emplace_back(Command{ManualCommand{.id = CommandId{99}, .instruction = std::format("N={}", i)}});
    }
    return Recipe{.id = RecipeId{name},
                  .display_name = std::move(name),
                  .description = std::move(description),
                  .commands = std::move(commands)};
}

IngredientStore store_with_rum()
{
    IngredientStore store;
    store.init_ingredients({Ingredient{.id = IngredientId{"rum"}, .display_name = "Rum", .type = {}}});
    return store;
}

std::vector<Recipe> three_recipes()
{
    return {
        make_recipe("Pasta Carbonara"),
        make_recipe("Beef Stew"),
        make_recipe("Lemon Tart"),
    };
}

} // namespace

TEST_CASE("Recipe can be formatted.", "[Recipe]")
{
    Recipe recipe{.id = RecipeId{"Id"}, .display_name = "DisplayName"};

    const auto formatted_str = std::format("Test={}", recipe);
    CHECK(formatted_str == "Test=Recipe(id=Id, name=DisplayName)");
}

// ---------------------------------------------------------------------------
// Construction & recipe_count
// ---------------------------------------------------------------------------

TEST_CASE("RecipeStore construction", "[RecipeStore]")
{
    SECTION("empty store has count of zero")
    {
        RecipeStore store{};
        CHECK(store.recipe_count() == 0);
    }

    SECTION("store reports correct count for non-empty input")
    {
        RecipeStore store{};
        store.init_recipes(three_recipes());
        CHECK(store.recipe_count() == 3);
    }

    SECTION("ids are assigned")
    {
        const auto recipes = three_recipes();
        RecipeStore store{};
        store.init_recipes(recipes);
        for (std::size_t i = 0; i < store.recipe_count(); ++i) {
            auto&& ref = recipes.at(i);
            auto recipe = store.find_by_id(ref.id);
            REQUIRE(recipe.has_value());
            CHECK(recipe->id == ref.id);
        }
    }

    SECTION("command ids are assigned sequentially starting from 0")
    {
        RecipeStore store{};
        store.init_recipes({make_recipe("Quattro Frommagi", "4 Käse", 4)});
        auto recipe = store.find_by_id(RecipeId{"Quattro Frommagi"});
        REQUIRE(recipe.has_value());
        REQUIRE(recipe->commands.size() == 4);
        for (int i = 1; i <= recipe->commands.size(); ++i) {
            auto* common_cmd = std::get_if<Command>(&recipe->commands[i - 1]);
            REQUIRE(common_cmd != nullptr);
            auto* cmd = std::get_if<ManualCommand>(common_cmd);
            REQUIRE(cmd != nullptr);
            CHECK(cmd->id == CommandId{i});
        }
    }

    SECTION("recipe data is preserved after construction")
    {
        RecipeStore store{};
        store.init_recipes({make_recipe("Pasta Carbonara", "Classic Roman pasta")});
        auto recipe = store.find_by_id(RecipeId{"Pasta Carbonara"});
        REQUIRE(recipe.has_value());
        CHECK(recipe->id == RecipeId{"Pasta Carbonara"});
        CHECK(recipe->display_name == "Pasta Carbonara");
        CHECK(recipe->description == "Classic Roman pasta");
    }
}

// ---------------------------------------------------------------------------
// find_by_id
// ---------------------------------------------------------------------------

TEST_CASE("RecipeStore::find_by_id", "[RecipeStore]")
{
    RecipeStore store{};
    store.init_recipes(three_recipes());

    SECTION("returns recipe for valid id")
    {
        auto result = store.find_by_id(RecipeId{"Pasta Carbonara"});
        REQUIRE(result.has_value());
        CHECK(result->display_name == "Pasta Carbonara");
    }

    SECTION("returns nullopt for id equal to recipe_count (one past end)")
    {
        CHECK_FALSE(store.find_by_id(RecipeId{"AlreadyEaten"}).has_value());
    }

    SECTION("returns nullopt on empty store")
    {
        RecipeStore empty{};
        CHECK_FALSE(empty.find_by_id(RecipeId{"Is there someone?"}).has_value());
    }
}

// ---------------------------------------------------------------------------
// find_by_index
// ---------------------------------------------------------------------------

TEST_CASE("RecipeStore::find_by_index", "[RecipeStore]")
{
    RecipeStore store{};
    store.init_recipes(three_recipes());

    SECTION("returns recipe at valid index")
    {
        auto result = store.find_by_index(1);
        REQUIRE(result.has_value());
        CHECK(result->display_name == "Beef Stew");
    }

    SECTION("returns recipe at index 0")
    {
        auto result = store.find_by_index(0);
        REQUIRE(result.has_value());
        CHECK(result->display_name == "Pasta Carbonara");
    }

    SECTION("returns recipe at last valid index")
    {
        auto result = store.find_by_index(store.recipe_count() - 1);
        REQUIRE(result.has_value());
        CHECK(result->display_name == "Lemon Tart");
    }

    SECTION("returns nullopt for index == recipe_count")
    {
        CHECK_FALSE(store.find_by_index(store.recipe_count()).has_value());
    }

    SECTION("returns nullopt for large out-of-range index")
    {
        CHECK_FALSE(store.find_by_index(9999).has_value());
    }

    SECTION("returns nullopt on empty store")
    {
        RecipeStore empty{};
        CHECK_FALSE(empty.find_by_index(0).has_value());
    }
}

// ---------------------------------------------------------------------------
// load_recipes_from_dir
// ---------------------------------------------------------------------------

using Catch::Matchers::ContainsSubstring;

TEST_CASE("load_recipes_from_dir refuses a recipe without a serving volume", "[recipe][load]")
{
    const cm::testing::TempDir dir{"cm-recipe-test"};
    const auto store = store_with_rum();
    dir.write_file("no_volume.json", R"({"name": "X", "steps": [{"kind": "dispense", "ingredient": "rum", "volume": 40}]})");

    CHECK_THROWS_WITH(load_recipes_from_dir(dir.path(), store),
                      ContainsSubstring("no_volume.json") && ContainsSubstring("nominal_serving_volume"));
}

TEST_CASE("load_recipes_from_dir refuses a serving volume of 0", "[recipe][load]")
{
    const cm::testing::TempDir dir{"cm-recipe-test"};
    const auto store = store_with_rum();
    dir.write_file("zero.json",
                   R"({"name": "X", "nominal_serving_volume": 0,
                  "steps": [{"kind": "dispense", "ingredient": "rum", "volume": 40}]})");

    CHECK_THROWS_WITH(load_recipes_from_dir(dir.path(), store), ContainsSubstring("nominal_serving_volume"));
}

TEST_CASE("load_recipes_from_dir refuses a dispense step below 1 mL", "[recipe][load]")
{
    const cm::testing::TempDir dir{"cm-recipe-test"};
    const auto store = store_with_rum();
    dir.write_file("tiny.json",
                   R"({"name": "X", "nominal_serving_volume": 200,
                  "steps": [{"kind": "parallel", "steps": [{"kind": "dispense", "ingredient": "rum", "volume": 0}]}]})");

    CHECK_THROWS_WITH(load_recipes_from_dir(dir.path(), store), ContainsSubstring("at least 1 mL"));
}

TEST_CASE("load_recipes_from_dir reads the serving volume", "[recipe][load]")
{
    const cm::testing::TempDir dir{"cm-recipe-test"};
    const auto store = store_with_rum();
    dir.write_file("good.json",
                   R"({"name": "X", "nominal_serving_volume": 200,
                  "steps": [{"kind": "dispense", "ingredient": "rum", "volume": 40}]})");

    const auto recipes = load_recipes_from_dir(dir.path(), store);

    REQUIRE(recipes.size() == 1);
    CHECK(recipes.front().nominal_serving_volume == 200 * units::milli_litre);
}
