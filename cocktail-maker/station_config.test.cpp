#include <catch2/catch_test_macros.hpp>
import std;
import cm.core;
import cm.testing;
import cm;

using namespace cm;

namespace {
struct StationConfigFixture
{
    cm::testing::TempDir dir;
    IngredientStore ingredients;
    const PodDispenser address{PodId{"Pod"}, DispenserId{2}};

    StationConfigFixture()
    {
        ingredients.init_ingredients({Ingredient{.id = IngredientId{"rum"}, .display_name = "Rum", .type = {}}});
    }
};
} // namespace

TEST_CASE_METHOD(StationConfigFixture, "StationConfig keeps an assignment across a restart", "[station_config]")
{
    {
        StationConfig config{ingredients, dir.file("mapping.json")};
        config.update_dispenser_ingredient_mapping(IngredientId{"rum"}, address);
        config.flush();
    }

    StationConfig reloaded{ingredients, dir.file("mapping.json")};
    reloaded.init();

    const auto found = reloaded.find_dispenser_for_ingredient(IngredientId{"rum"});
    REQUIRE(found.has_value());
    CHECK(*found == address);
}

TEST_CASE_METHOD(StationConfigFixture, "StationConfig writes nothing but the file it was given", "[station_config]")
{
    {
        StationConfig config{ingredients, dir.file("mapping.json")};
        config.update_dispenser_ingredient_mapping(IngredientId{"rum"}, address);
    }

    std::vector<std::string> files;
    for (const auto& entry : std::filesystem::directory_iterator{dir.path()}) {
        files.push_back(entry.path().filename().string());
    }
    CHECK(files == std::vector<std::string>{"mapping.json"});
}

TEST_CASE_METHOD(StationConfigFixture, "StationConfig starts empty without a file", "[station_config]")
{
    StationConfig config{ingredients, dir.file("mapping.json")};
    config.init();

    CHECK_FALSE(config.find_dispenser_for_ingredient(IngredientId{"rum"}).has_value());
}
