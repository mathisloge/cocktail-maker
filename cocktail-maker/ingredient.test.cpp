#include <catch2/catch_test_macros.hpp>

import cm;

using namespace cm;

namespace {

Ingredient make_ingredient(IngredientId id,
                           std::string display_name = "Test Ingredient",
                           IngredientType type = IngredientType::other,
                           BoostCategory boost_category = BoostCategory::fixed)
{
    return Ingredient{
        .id = std::move(id),
        .display_name = std::move(display_name),
        .type = type,
        .boost_category = boost_category,
    };
}

} // namespace

TEST_CASE("IngredientStore::add returns true when ingredient is new", "[IngredientStore][add]")
{
    IngredientStore store;
    const auto ingredient = make_ingredient("rum-white");
    CHECK(store.add(ingredient));
}

TEST_CASE("IngredientStore::add returns false for a duplicate id", "[IngredientStore][add]")
{
    IngredientStore store;
    const auto ingredient = make_ingredient("rum-white");
    REQUIRE(store.add(ingredient));

    // Second insertion with the same id must fail.
    CHECK_FALSE(store.add(ingredient));
}

TEST_CASE("IngredientStore::add does not overwrite existing ingredient on duplicate id", "[IngredientStore][add]")
{
    IngredientStore store;
    REQUIRE(store.add(make_ingredient("lime-juice", "Lime Juice", IngredientType::juice)));

    // Attempt to overwrite with a different display_name.
    REQUIRE_FALSE(store.add(make_ingredient("lime-juice", "OVERWRITTEN", IngredientType::other)));

    const auto result = store.find_by_id("lime-juice");
    REQUIRE(result.has_value());
    CHECK(result->display_name == "Lime Juice");
}

TEST_CASE("IngredientStore::add accepts multiple distinct ingredients", "[IngredientStore][add]")
{
    IngredientStore store;
    CHECK(store.add(make_ingredient("rum-white")));
    CHECK(store.add(make_ingredient("lime-juice")));
    CHECK(store.add(make_ingredient("simple-syrup")));
}

TEST_CASE("IngredientStore::find_by_id returns nullopt for unknown id", "[IngredientStore][find_by_id]")
{
    IngredientStore store;
    CHECK_FALSE(store.find_by_id("does-not-exist").has_value());
}

TEST_CASE("IngredientStore::find_by_id returns the correct ingredient after add", "[IngredientStore][find_by_id]")
{
    IngredientStore store;
    const Ingredient expected{
        .id = "grenadine",
        .display_name = "Grenadine",
        .type = IngredientType::syrup,
        .boost_category = BoostCategory::reducible,
    };
    REQUIRE(store.add(expected));

    const auto result = store.find_by_id("grenadine");
    REQUIRE(result.has_value());
    CHECK(result->id == expected.id);
    CHECK(result->display_name == expected.display_name);
    CHECK(result->type == expected.type);
    CHECK(result->boost_category == expected.boost_category);
}

TEST_CASE("IngredientStore::find_by_id can retrieve each of several stored ingredients", "[IngredientStore][find_by_id]")
{
    IngredientStore store;
    REQUIRE(store.add(make_ingredient("vodka", "Vodka", IngredientType::alcohol, BoostCategory::boostable)));
    REQUIRE(store.add(make_ingredient("tonic", "Tonic Water", IngredientType::soda, BoostCategory::reducible)));
    REQUIRE(store.add(make_ingredient("mint", "Mint Leaves", IngredientType::other, BoostCategory::fixed)));

    CHECK(store.find_by_id("vodka")->display_name == "Vodka");
    CHECK(store.find_by_id("tonic")->display_name == "Tonic Water");
    CHECK(store.find_by_id("mint")->display_name == "Mint Leaves");
}

TEST_CASE("IngredientStore::find_by_id returns a copy, not a reference", "[IngredientStore][find_by_id]")
{
    IngredientStore store;
    REQUIRE(store.add(make_ingredient("angostura", "Angostura Bitters", IngredientType::bitters)));

    auto copy = store.find_by_id("angostura");
    REQUIRE(copy.has_value());

    // Mutating the returned copy must not affect the stored value.
    copy->display_name = "Modified";

    const auto stored = store.find_by_id("angostura");
    REQUIRE(stored.has_value());
    CHECK(stored->display_name == "Angostura Bitters");
}

// Round-trip tests

TEST_CASE("IngredientStore preserves all IngredientType values", "[IngredientStore][round-trip]")
{
    using IT = IngredientType;
    constexpr std::pair<IngredientId, IT> cases[] = {
        {"a", IT::alcohol},
        {"j", IT::juice},
        {"s", IT::syrup},
        {"so", IT::soda},
        {"w", IT::water},
        {"d", IT::dairy},
        {"b", IT::bitters},
        {"p", IT::puree},
        {"o", IT::other},
    };

    IngredientStore store;
    for (const auto& [id, type] : cases) {
        REQUIRE(store.add(make_ingredient(id, id, type)));
    }

    for (const auto& [id, type] : cases) {
        const auto result = store.find_by_id(id);
        REQUIRE(result.has_value());
        CHECK(result->type == type);
    }
}

TEST_CASE("IngredientStore preserves all BoostCategory values", "[IngredientStore][round-trip]")
{
    using BC = BoostCategory;
    constexpr std::pair<IngredientId, BC> cases[] = {
        {"fixed-id", BC::fixed},
        {"boost-id", BC::boostable},
        {"reduce-id", BC::reducible},
    };

    IngredientStore store;
    for (const auto& [id, bc] : cases) {
        REQUIRE(store.add(make_ingredient(id, id, IngredientType::other, bc)));
    }

    for (const auto& [id, bc] : cases) {
        const auto result = store.find_by_id(id);
        REQUIRE(result.has_value());
        CHECK(result->boost_category == bc);
    }
}
