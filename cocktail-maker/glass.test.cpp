#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

import std;
import mp_units;
import cm;
import cm.core;
import cm.testing;

using namespace cm;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kVolumeToleranceMl = 1e-9;

constexpr units::Litre ml(double value)
{
    return value * units::milli_litre;
}

std::vector<double> to_ml(const std::vector<units::Litre>& volumes)
{
    std::vector<double> result;
    result.reserve(volumes.size());
    for (const auto& volume : volumes) {
        result.push_back(volume.numerical_value_in(units::milli_litre));
    }
    return result;
}

void check_volumes_ml(const std::vector<units::Litre>& volumes, const std::vector<double>& expected_ml)
{
    const auto actual_ml = to_ml(volumes);
    REQUIRE(actual_ml.size() == expected_ml.size());
    for (const auto& [actual, expected] : std::views::zip(actual_ml, expected_ml)) {
        CHECK_THAT(actual, WithinAbs(expected, kVolumeToleranceMl));
    }
}

Glass make_glass(std::string id, std::vector<units::Litre> active_volumes = {})
{
    return Glass{
        .id = GlassId{id},
        .display_name = std::move(id),
        .common_volumes = {ml(200.0), ml(300.0)},
        .active_volumes = std::move(active_volumes),
        .icon = GlassIconData{.viewbox_width = 100.F, .viewbox_height = 180.F, .outline = "M 0 0 L 1 1", .rim = {}},
    };
}

std::string glass_json(std::string_view id, std::string_view display_name, std::string_view common_volumes_ml = "[200, 300]")
{
    return std::format(R"({{
        "id": "{}",
        "display_name": "{}",
        "common_volumes_ml": {},
        "icon": {{
            "viewbox_width": 100,
            "viewbox_height": 180,
            "outline": "M 22 18 L 22 158",
            "rim": ["M 18 18 L 82 18", "M 22 26 L 78 26"]
        }}
    }})",
                       id,
                       display_name,
                       common_volumes_ml);
}

} // namespace

// ── GlassStore: init & lookup ─────────────────────────────────────────────────

TEST_CASE("GlassStore::glasses is empty for a fresh store", "[GlassStore][init_glasses]")
{
    const cm::testing::TempDir dir;

    const GlassStore store{dir.file("glass_active_volumes.json")};
    CHECK(store.glasses().empty());
}

TEST_CASE("GlassStore::init_glasses keys the glasses by their id", "[GlassStore][init_glasses]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball"), make_glass("wine"), make_glass("margarita")});

    const auto& glasses = store.glasses();
    REQUIRE(glasses.size() == 3);
    CHECK(glasses.contains(GlassId{"highball"}));
    CHECK(glasses.contains(GlassId{"wine"}));
    CHECK(glasses.contains(GlassId{"margarita"}));
    CHECK_FALSE(glasses.contains(GlassId{"tumbler"}));
}

TEST_CASE("GlassStore::init_glasses preserves all glass fields", "[GlassStore][init_glasses]")
{
    const cm::testing::TempDir dir;

    Glass expected = make_glass("coupe");
    expected.display_name = "Coupe Glas";
    expected.icon.rim = {"M 18 18 L 82 18"};

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({expected});

    const auto it = store.glasses().find(GlassId{"coupe"});
    REQUIRE(it != store.glasses().end());
    const auto& stored = it->second;
    CHECK(stored.id == expected.id);
    CHECK(stored.display_name == "Coupe Glas");
    check_volumes_ml(stored.common_volumes, {200.0, 300.0});
    CHECK(stored.icon.viewbox_width == expected.icon.viewbox_width);
    CHECK(stored.icon.viewbox_height == expected.icon.viewbox_height);
    CHECK(stored.icon.outline == expected.icon.outline);
    CHECK(stored.icon.rim == expected.icon.rim);
}

TEST_CASE("GlassStore::init_glasses refuses a duplicate id", "[GlassStore][init_glasses]")
{
    const cm::testing::TempDir dir;
    GlassStore store{dir.file("glass_active_volumes.json")};

    CHECK_THROWS_AS(store.init_glasses({make_glass("highball"), make_glass("highball")}), std::invalid_argument);
    CHECK_THROWS_WITH(store.init_glasses({make_glass("highball"), make_glass("highball")}),
                      Catch::Matchers::ContainsSubstring("highball"));
}

TEST_CASE("GlassStore::init_glasses replaces previously stored glasses", "[GlassStore][init_glasses]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball"), make_glass("wine")});
    store.init_glasses({make_glass("tumbler")});

    const auto& glasses = store.glasses();
    REQUIRE(glasses.size() == 1);
    CHECK(glasses.contains(GlassId{"tumbler"}));
    CHECK_FALSE(glasses.contains(GlassId{"highball"}));
}

// ── GlassStore: active volumes ────────────────────────────────────────────────

TEST_CASE("GlassStore::add_active_volume adds a volume to a known glass", "[GlassStore][add_active_volume]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball")});

    CHECK(store.add_active_volume(GlassId{"highball"}, ml(300.0)));
    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {300.0});
}

TEST_CASE("GlassStore::add_active_volume keeps the active volumes sorted", "[GlassStore][add_active_volume]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball")});

    CHECK(store.add_active_volume(GlassId{"highball"}, ml(400.0)));
    CHECK(store.add_active_volume(GlassId{"highball"}, ml(200.0)));
    CHECK(store.add_active_volume(GlassId{"highball"}, ml(300.0)));

    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {200.0, 300.0, 400.0});
}

TEST_CASE("GlassStore::add_active_volume rejects a duplicate volume", "[GlassStore][add_active_volume]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball")});

    REQUIRE(store.add_active_volume(GlassId{"highball"}, ml(300.0)));
    CHECK_FALSE(store.add_active_volume(GlassId{"highball"}, ml(300.0)));

    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {300.0});
}

TEST_CASE("GlassStore::add_active_volume ignores an unknown glass", "[GlassStore][add_active_volume]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball")});

    CHECK_FALSE(store.add_active_volume(GlassId{"does-not-exist"}, ml(300.0)));
    CHECK(store.glasses().at(GlassId{"highball"}).active_volumes.empty());
    CHECK(store.glasses().size() == 1);
}

TEST_CASE("GlassStore::add_active_volume only touches the addressed glass", "[GlassStore][add_active_volume]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball"), make_glass("wine")});

    REQUIRE(store.add_active_volume(GlassId{"highball"}, ml(300.0)));

    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {300.0});
    CHECK(store.glasses().at(GlassId{"wine"}).active_volumes.empty());
}

TEST_CASE("GlassStore::remove_active_volume removes an existing volume", "[GlassStore][remove_active_volume]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball", {ml(200.0), ml(300.0), ml(400.0)})});

    CHECK(store.remove_active_volume(GlassId{"highball"}, ml(300.0)));
    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {200.0, 400.0});
}

TEST_CASE("GlassStore::remove_active_volume ignores a volume that is not active", "[GlassStore][remove_active_volume]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball", {ml(200.0)})});

    CHECK_FALSE(store.remove_active_volume(GlassId{"highball"}, ml(500.0)));
    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {200.0});
}

TEST_CASE("GlassStore::remove_active_volume ignores an unknown glass", "[GlassStore][remove_active_volume]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball", {ml(200.0)})});

    CHECK_FALSE(store.remove_active_volume(GlassId{"does-not-exist"}, ml(200.0)));
    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {200.0});
}

TEST_CASE("GlassStore active volumes survive add/remove cycles", "[GlassStore][add_active_volume][remove_active_volume]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball")});

    REQUIRE(store.add_active_volume(GlassId{"highball"}, ml(300.0)));
    REQUIRE(store.remove_active_volume(GlassId{"highball"}, ml(300.0)));
    CHECK(store.glasses().at(GlassId{"highball"}).active_volumes.empty());
    CHECK(store.add_active_volume(GlassId{"highball"}, ml(300.0)));
    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {300.0});
}

// ── Active volume persistence ─────────────────────────────────────────────────

TEST_CASE("Added active volumes are persisted and reloaded by a new store", "[GlassStore][persistence]")
{
    const cm::testing::TempDir dir;

    {
        GlassStore store{dir.file("glass_active_volumes.json")};
        store.init_glasses({make_glass("highball"), make_glass("wine")});
        REQUIRE(store.add_active_volume(GlassId{"highball"}, ml(400.0)));
        REQUIRE(store.add_active_volume(GlassId{"highball"}, ml(200.0)));
        REQUIRE(store.add_active_volume(GlassId{"wine"}, ml(150.0)));
    }

    GlassStore reloaded{dir.file("glass_active_volumes.json")};
    reloaded.init_glasses({make_glass("highball"), make_glass("wine")});

    check_volumes_ml(reloaded.glasses().at(GlassId{"highball"}).active_volumes, {200.0, 400.0});
    check_volumes_ml(reloaded.glasses().at(GlassId{"wine"}).active_volumes, {150.0});
}

TEST_CASE("Removed active volumes are persisted", "[GlassStore][persistence]")
{
    const cm::testing::TempDir dir;

    {
        GlassStore store{dir.file("glass_active_volumes.json")};
        store.init_glasses({make_glass("highball", {ml(200.0), ml(300.0)})});
        REQUIRE(store.remove_active_volume(GlassId{"highball"}, ml(200.0)));
    }

    GlassStore reloaded{dir.file("glass_active_volumes.json")};
    reloaded.init_glasses({make_glass("highball", {ml(200.0), ml(300.0)})});

    check_volumes_ml(reloaded.glasses().at(GlassId{"highball"}).active_volumes, {300.0});
}

TEST_CASE("GlassStore saves the active volumes of every glass, an empty list too", "[GlassStore][persistence]")
{
    const cm::testing::TempDir dir;

    {
        GlassStore store{dir.file("glass_active_volumes.json")};
        store.init_glasses({make_glass("highball", {ml(200.0)}), make_glass("wine")});
        REQUIRE(store.add_active_volume(GlassId{"highball"}, ml(350.5)));
    }

    GlassStore reloaded{dir.file("glass_active_volumes.json")};
    reloaded.init_glasses({make_glass("highball"), make_glass("wine", {ml(999.0)})});

    check_volumes_ml(reloaded.glasses().at(GlassId{"highball"}).active_volumes, {200.0, 350.5});
    CHECK(reloaded.glasses().at(GlassId{"wine"}).active_volumes.empty());
}

TEST_CASE("GlassStore keeps the catalog's volumes when nothing is stored", "[GlassStore][persistence]")
{
    const cm::testing::TempDir dir;

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball", {ml(250.0)})});

    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {250.0});
}

TEST_CASE("GlassStore sorts and de-duplicates the stored volumes", "[GlassStore][persistence]")
{
    const cm::testing::TempDir dir;
    dir.write_file("glass_active_volumes.json",
                   R"({"version": 1, "glasses": [{"glass_id": "highball", "active_volumes": [400, 200, 400, 300]}]})");

    GlassStore store{dir.file("glass_active_volumes.json")};
    store.init_glasses({make_glass("highball")});

    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {200.0, 300.0, 400.0});
}

TEST_CASE("GlassStore skips stored entries for unknown glasses", "[GlassStore][persistence]")
{
    const cm::testing::TempDir dir;
    dir.write_file("glass_active_volumes.json",
                   R"({"version": 1, "glasses": [{"glass_id": "gone", "active_volumes": [100]},
                       {"glass_id": "highball", "active_volumes": [300]}]})");

    GlassStore store{dir.file("glass_active_volumes.json")};
    REQUIRE_NOTHROW(store.init_glasses({make_glass("highball")}));

    CHECK(store.glasses().size() == 1);
    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {300.0});
}

TEST_CASE("GlassStore keeps the catalog's volumes for a malformed file", "[GlassStore][persistence]")
{
    const cm::testing::TempDir dir;
    dir.write_file("glass_active_volumes.json", R"({"version": 1, "not-glasses": [)");

    GlassStore store{dir.file("glass_active_volumes.json")};
    REQUIRE_NOTHROW(store.init_glasses({make_glass("highball", {ml(250.0)})}));

    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {250.0});
}

// ── load_glasses_from_dir ─────────────────────────────────────────────────────

TEST_CASE("load_glasses_from_dir returns nothing for an empty directory", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    CHECK(load_glasses_from_dir(dir.path()).empty());
}

TEST_CASE("load_glasses_from_dir parses every field of a glass file", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    dir.write_file("highball.json", glass_json("highball", "Highball Glas", "[300, 350, 500]"));

    const auto glasses = load_glasses_from_dir(dir.path());
    REQUIRE(glasses.size() == 1);

    const auto& glass = glasses.front();
    CHECK(glass.id == GlassId{"highball"});
    CHECK(glass.display_name == "Highball Glas");
    check_volumes_ml(glass.common_volumes, {300.0, 350.0, 500.0});
    CHECK(glass.icon.viewbox_width == 100.F);
    CHECK(glass.icon.viewbox_height == 180.F);
    CHECK(glass.icon.outline == "M 22 18 L 22 158");
    CHECK(glass.icon.rim == std::vector<std::string>{"M 18 18 L 82 18", "M 22 26 L 78 26"});
}

TEST_CASE("load_glasses_from_dir leaves active volumes empty", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    dir.write_file("highball.json", glass_json("highball", "Highball Glas"));

    const auto glasses = load_glasses_from_dir(dir.path());
    REQUIRE(glasses.size() == 1);
    CHECK(glasses.front().active_volumes.empty());
}

TEST_CASE("load_glasses_from_dir treats the icon rim as optional", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    dir.write_file("willi.json", R"({
        "id": "willi",
        "display_name": "Willi Becher",
        "common_volumes_ml": [400],
        "icon": {"viewbox_width": 90, "viewbox_height": 170, "outline": "M 0 0 L 1 1"}
    })");

    const auto glasses = load_glasses_from_dir(dir.path());
    REQUIRE(glasses.size() == 1);
    CHECK(glasses.front().icon.rim.empty());
}

TEST_CASE("load_glasses_from_dir reads all json files in the directory", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    dir.write_file("highball.json", glass_json("highball", "Highball Glas"));
    dir.write_file("wine.json", glass_json("wine", "Weinglas"));
    dir.write_file("margarita.json", glass_json("margarita", "Margarita Glas"));

    auto glasses = load_glasses_from_dir(dir.path());
    REQUIRE(glasses.size() == 3);

    std::ranges::sort(glasses, {}, [](const Glass& glass) { return glass.id.raw(); });
    CHECK(glasses.at(0).id == GlassId{"highball"});
    CHECK(glasses.at(1).id == GlassId{"margarita"});
    CHECK(glasses.at(2).id == GlassId{"wine"});
}

TEST_CASE("load_glasses_from_dir returns the glasses in file name order", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    dir.write_file("b.json", glass_json("wine", "Wine"));
    dir.write_file("a.json", glass_json("highball", "Highball"));
    dir.write_file("c.json", glass_json("tumbler", "Tumbler"));

    const auto glasses = load_glasses_from_dir(dir.path());

    REQUIRE(glasses.size() == 3);
    CHECK(glasses[0].id == GlassId{"highball"});
    CHECK(glasses[1].id == GlassId{"wine"});
    CHECK(glasses[2].id == GlassId{"tumbler"});
}

TEST_CASE("load_glasses_from_dir ignores non-json files", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    dir.write_file("highball.json", glass_json("highball", "Highball Glas"));
    dir.write_file("README.md", "not a glass");
    dir.write_file("wine.json.bak", glass_json("wine", "Weinglas"));

    const auto glasses = load_glasses_from_dir(dir.path());
    REQUIRE(glasses.size() == 1);
    CHECK(glasses.front().id == GlassId{"highball"});
}

TEST_CASE("load_glasses_from_dir ignores subdirectories", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    dir.write_file("highball.json", glass_json("highball", "Highball Glas"));
    std::filesystem::create_directory(dir.path() / "nested.json");

    const auto glasses = load_glasses_from_dir(dir.path());
    REQUIRE(glasses.size() == 1);
    CHECK(glasses.front().id == GlassId{"highball"});
}

TEST_CASE("load_glasses_from_dir throws on a malformed glass file", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    dir.write_file("broken.json", R"({"id": "broken", "display_name": )");

    CHECK_THROWS_AS(load_glasses_from_dir(dir.path()), std::runtime_error);
}

TEST_CASE("load_glasses_from_dir throws when a required field is missing", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    dir.write_file("incomplete.json", R"({"id": "incomplete", "display_name": "No Icon", "common_volumes_ml": [200]})");

    CHECK_THROWS_AS(load_glasses_from_dir(dir.path()), std::runtime_error);
}

TEST_CASE("load_glasses_from_dir throws for a non-existing directory", "[load_glasses_from_dir]")
{
    const cm::testing::TempDir dir;
    CHECK_THROWS_AS(load_glasses_from_dir(dir.path() / "does-not-exist"), std::filesystem::filesystem_error);
}
