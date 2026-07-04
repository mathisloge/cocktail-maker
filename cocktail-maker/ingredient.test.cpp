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
    const auto ingredient = make_ingredient(IngredientId{"rum-white"});
    CHECK(store.add(ingredient));
}

TEST_CASE("IngredientStore::add returns false for a duplicate id", "[IngredientStore][add]")
{
    IngredientStore store;
    const auto ingredient = make_ingredient(IngredientId{"rum-white"});
    REQUIRE(store.add(ingredient));

    // Second insertion with the same id must fail.
    CHECK_FALSE(store.add(ingredient));
}

TEST_CASE("IngredientStore::add does not overwrite existing ingredient on duplicate id", "[IngredientStore][add]")
{
    IngredientStore store;
    REQUIRE(store.add(make_ingredient(IngredientId{"lime-juice"}, "Lime Juice", IngredientType::juice)));

    // Attempt to overwrite with a different display_name.
    REQUIRE_FALSE(store.add(make_ingredient(IngredientId{"lime-juice"}, "OVERWRITTEN", IngredientType::other)));

    const auto result = store.find_by_id(IngredientId{"lime-juice"});
    REQUIRE(result.has_value());
    CHECK(result->display_name == "Lime Juice");
}

TEST_CASE("IngredientStore::add accepts multiple distinct ingredients", "[IngredientStore][add]")
{
    IngredientStore store;
    CHECK(store.add(make_ingredient(IngredientId{"rum-white"})));
    CHECK(store.add(make_ingredient(IngredientId{"lime-juice"})));
    CHECK(store.add(make_ingredient(IngredientId{"simple-syrup"})));
}

TEST_CASE("IngredientStore::find_by_id returns nullopt for unknown id", "[IngredientStore][find_by_id]")
{
    IngredientStore store;
    CHECK_FALSE(store.find_by_id(IngredientId{"does-not-exist"}).has_value());
}

TEST_CASE("IngredientStore::find_by_id returns the correct ingredient after add", "[IngredientStore][find_by_id]")
{
    IngredientStore store;
    const Ingredient expected{
        .id = IngredientId{"grenadine"},
        .display_name = "Grenadine",
        .type = IngredientType::syrup,
        .boost_category = BoostCategory::reducible,
    };
    REQUIRE(store.add(expected));

    const auto result = store.find_by_id(IngredientId{"grenadine"});
    REQUIRE(result.has_value());
    CHECK(result->id == expected.id);
    CHECK(result->display_name == expected.display_name);
    CHECK(result->type == expected.type);
    CHECK(result->boost_category == expected.boost_category);
}

TEST_CASE("IngredientStore::find_by_id can retrieve each of several stored ingredients", "[IngredientStore][find_by_id]")
{
    IngredientStore store;
    REQUIRE(store.add(make_ingredient(IngredientId{"vodka"}, "Vodka", IngredientType::alcohol, BoostCategory::boostable)));
    REQUIRE(store.add(make_ingredient(IngredientId{"tonic"}, "Tonic Water", IngredientType::soda, BoostCategory::reducible)));
    REQUIRE(store.add(make_ingredient(IngredientId{"mint"}, "Mint Leaves", IngredientType::other, BoostCategory::fixed)));

    CHECK(store.find_by_id(IngredientId{"vodka"})->display_name == "Vodka");
    CHECK(store.find_by_id(IngredientId{"tonic"})->display_name == "Tonic Water");
    CHECK(store.find_by_id(IngredientId{"mint"})->display_name == "Mint Leaves");
}

TEST_CASE("IngredientStore::find_by_id returns a copy, not a reference", "[IngredientStore][find_by_id]")
{
    IngredientStore store;
    REQUIRE(store.add(make_ingredient(IngredientId{"angostura"}, "Angostura Bitters", IngredientType::bitters)));

    auto copy = store.find_by_id(IngredientId{"angostura"});
    REQUIRE(copy.has_value());

    // Mutating the returned copy must not affect the stored value.
    copy->display_name = "Modified";

    const auto stored = store.find_by_id(IngredientId{"angostura"});
    REQUIRE(stored.has_value());
    CHECK(stored->display_name == "Angostura Bitters");
}

TEST_CASE("IngredientStore::ingredients returns empty vector for empty store", "[IngredientStore][ingredients]")
{
    IngredientStore store;
    const auto result = store.ingredients();
    CHECK(result.empty());
}

TEST_CASE("IngredientStore::ingredients returns all ingredient ids", "[IngredientStore][ingredients]")
{
    IngredientStore store;
    const auto id1 = IngredientId{"rum-white"};
    const auto id2 = IngredientId{"lime-juice"};
    const auto id3 = IngredientId{"simple-syrup"};

    REQUIRE(store.add(make_ingredient(id1)));
    REQUIRE(store.add(make_ingredient(id2)));
    REQUIRE(store.add(make_ingredient(id3)));

    const auto result = store.ingredients();
    REQUIRE(result.size() == 3);

    // Check that all expected ids are present
    CHECK(std::find(result.begin(), result.end(), id1) != result.end());
    CHECK(std::find(result.begin(), result.end(), id2) != result.end());
    CHECK(std::find(result.begin(), result.end(), id3) != result.end());
}

// Round-trip tests

TEST_CASE("IngredientStore preserves all IngredientType values", "[IngredientStore][round-trip]")
{
    using IT = IngredientType;
    constexpr std::pair<IngredientId, IT> cases[] = {
        {IngredientId{"a"}, IT::alcohol},
        {IngredientId{"j"}, IT::juice},
        {IngredientId{"s"}, IT::syrup},
        {IngredientId{"so"}, IT::soda},
        {IngredientId{"w"}, IT::water},
        {IngredientId{"d"}, IT::dairy},
        {IngredientId{"b"}, IT::bitters},
        {IngredientId{"p"}, IT::puree},
        {IngredientId{"o"}, IT::other},
    };

    IngredientStore store;
    for (const auto& [id, type] : cases) {
        REQUIRE(store.add(make_ingredient(id, id.raw(), type)));
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
        {IngredientId{"fixed-id"}, BC::fixed},
        {IngredientId{"boost-id"}, BC::boostable},
        {IngredientId{"reduce-id"}, BC::reducible},
    };

    IngredientStore store;
    for (const auto& [id, bc] : cases) {
        REQUIRE(store.add(make_ingredient(id, id.raw(), IngredientType::other, bc)));
    }

    for (const auto& [id, bc] : cases) {
        const auto result = store.find_by_id(id);
        REQUIRE(result.has_value());
        CHECK(result->boost_category == bc);
    }
}
