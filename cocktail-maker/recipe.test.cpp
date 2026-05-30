#include <catch2/catch_test_macros.hpp>
import cm;
import std;
using namespace cm;

namespace {
Recipe make_recipe(std::string name, std::string description = "")
{
    return Recipe{.display_name = std::move(name), .description = std::move(description)};
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
    Recipe recipe;

    const auto formatted_str = std::format("Test={}", recipe);
    CHECK(formatted_str == "Test=Recipe(name=)");
}

// ---------------------------------------------------------------------------
// Construction & recipe_count
// ---------------------------------------------------------------------------

TEST_CASE("RecipeStore construction", "[RecipeStore]")
{
    SECTION("empty store has count of zero")
    {
        RecipeStore store{{}};
        CHECK(store.recipe_count() == 0);
    }

    SECTION("store reports correct count for non-empty input")
    {
        RecipeStore store{three_recipes()};
        CHECK(store.recipe_count() == 3);
    }

    SECTION("ids are assigned sequentially starting from 0")
    {
        RecipeStore store{three_recipes()};
        for (std::size_t i = 0; i < store.recipe_count(); ++i) {
            auto recipe = store.find_by_id(static_cast<int>(i));
            REQUIRE(recipe.has_value());
            CHECK(recipe->id == static_cast<int>(i));
        }
    }

    SECTION("recipe data is preserved after construction")
    {
        RecipeStore store{{make_recipe("Pasta Carbonara", "Classic Roman pasta")}};
        auto recipe = store.find_by_id(0);
        REQUIRE(recipe.has_value());
        CHECK(recipe->display_name == "Pasta Carbonara");
        CHECK(recipe->description == "Classic Roman pasta");
    }
}

// ---------------------------------------------------------------------------
// find_by_id
// ---------------------------------------------------------------------------

TEST_CASE("RecipeStore::find_by_id", "[RecipeStore]")
{
    RecipeStore store{three_recipes()};

    SECTION("returns recipe for valid id")
    {
        auto result = store.find_by_id(0);
        REQUIRE(result.has_value());
        CHECK(result->display_name == "Pasta Carbonara");
    }

    SECTION("returns last recipe by id")
    {
        auto result = store.find_by_id(2);
        REQUIRE(result.has_value());
        CHECK(result->display_name == "Lemon Tart");
    }

    SECTION("returns nullopt for id equal to recipe_count (one past end)")
    {
        CHECK_FALSE(store.find_by_id(static_cast<int>(store.recipe_count())).has_value());
    }

    SECTION("returns nullopt for negative id")
    {
        CHECK_FALSE(store.find_by_id(-1).has_value());
    }

    SECTION("returns nullopt on empty store")
    {
        RecipeStore empty{{}};
        CHECK_FALSE(empty.find_by_id(0).has_value());
    }
}

// ---------------------------------------------------------------------------
// find_by_index
// ---------------------------------------------------------------------------

TEST_CASE("RecipeStore::find_by_index", "[RecipeStore]")
{
    RecipeStore store{three_recipes()};

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
        RecipeStore empty{{}};
        CHECK_FALSE(empty.find_by_index(0).has_value());
    }
}

// ---------------------------------------------------------------------------
// find_by_id / find_by_index equivalence
// ---------------------------------------------------------------------------

TEST_CASE("RecipeStore::find_by_id and RecipeStore::find_by_index return the same recipe", "[RecipeStore]")
{
    // Since ids are assigned sequentially from 0, find_by_id(n) must equal
    // find_by_index(n) for all valid n.
    RecipeStore store{three_recipes()};

    for (std::size_t i = 0; i < store.recipe_count(); ++i) {
        auto by_id = store.find_by_id(static_cast<int>(i));
        auto by_index = store.find_by_index(i);
        REQUIRE(by_id.has_value());
        REQUIRE(by_index.has_value());
        CHECK(by_id->id == by_index->id);
        CHECK(by_id->display_name == by_index->display_name);
    }
}
