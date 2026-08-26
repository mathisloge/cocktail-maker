#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

import std;
import mp_units;
import cm;
import cm.core;

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

// `GlassStore` and the active-volume config functions read and write
// `glass_active_volumes.json` relative to the current working directory, so each test runs
// inside its own temporary directory to stay isolated.
class TempWorkingDir
{
  public:
    TempWorkingDir()
        : previous_{std::filesystem::current_path()}
        , dir_{unique_path()}
    {
        std::filesystem::create_directories(dir_);
        std::filesystem::current_path(dir_);
    }

    TempWorkingDir(const TempWorkingDir&) = delete;
    TempWorkingDir& operator=(const TempWorkingDir&) = delete;
    TempWorkingDir(TempWorkingDir&&) = delete;
    TempWorkingDir& operator=(TempWorkingDir&&) = delete;

    ~TempWorkingDir()
    {
        std::error_code ec;
        std::filesystem::current_path(previous_, ec);
        std::filesystem::remove_all(dir_, ec);
    }

    [[nodiscard]] const std::filesystem::path& path() const
    {
        return dir_;
    }

    void write_file(const std::string& name, std::string_view content) const
    {
        std::ofstream ofs{dir_ / name, std::ios::binary | std::ios::trunc};
        REQUIRE(ofs);
        ofs << content;
        REQUIRE(ofs);
    }

  private:
    static std::filesystem::path unique_path()
    {
        static std::atomic<std::uint64_t> counter{0};
        const auto stamp = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        return std::filesystem::temp_directory_path() / std::format("cm-glass-test-{}-{}", stamp, counter++);
    }

    std::filesystem::path previous_;
    std::filesystem::path dir_;
};

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
    const TempWorkingDir cwd;

    const GlassStore store;
    CHECK(store.glasses().empty());
}

TEST_CASE("GlassStore::init_glasses keys the glasses by their id", "[GlassStore][init_glasses]")
{
    const TempWorkingDir cwd;

    GlassStore store;
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
    const TempWorkingDir cwd;

    Glass expected = make_glass("coupe");
    expected.display_name = "Coupe Glas";
    expected.icon.rim = {"M 18 18 L 82 18"};

    GlassStore store;
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

TEST_CASE("GlassStore::init_glasses replaces previously stored glasses", "[GlassStore][init_glasses]")
{
    const TempWorkingDir cwd;

    GlassStore store;
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
    const TempWorkingDir cwd;

    GlassStore store;
    store.init_glasses({make_glass("highball")});

    CHECK(store.add_active_volume(GlassId{"highball"}, ml(300.0)));
    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {300.0});
}

TEST_CASE("GlassStore::add_active_volume keeps the active volumes sorted", "[GlassStore][add_active_volume]")
{
    const TempWorkingDir cwd;

    GlassStore store;
    store.init_glasses({make_glass("highball")});

    CHECK(store.add_active_volume(GlassId{"highball"}, ml(400.0)));
    CHECK(store.add_active_volume(GlassId{"highball"}, ml(200.0)));
    CHECK(store.add_active_volume(GlassId{"highball"}, ml(300.0)));

    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {200.0, 300.0, 400.0});
}

TEST_CASE("GlassStore::add_active_volume rejects a duplicate volume", "[GlassStore][add_active_volume]")
{
    const TempWorkingDir cwd;

    GlassStore store;
    store.init_glasses({make_glass("highball")});

    REQUIRE(store.add_active_volume(GlassId{"highball"}, ml(300.0)));
    CHECK_FALSE(store.add_active_volume(GlassId{"highball"}, ml(300.0)));

    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {300.0});
}

TEST_CASE("GlassStore::add_active_volume ignores an unknown glass", "[GlassStore][add_active_volume]")
{
    const TempWorkingDir cwd;

    GlassStore store;
    store.init_glasses({make_glass("highball")});

    CHECK_FALSE(store.add_active_volume(GlassId{"does-not-exist"}, ml(300.0)));
    CHECK(store.glasses().at(GlassId{"highball"}).active_volumes.empty());
    CHECK(store.glasses().size() == 1);
}

TEST_CASE("GlassStore::add_active_volume only touches the addressed glass", "[GlassStore][add_active_volume]")
{
    const TempWorkingDir cwd;

    GlassStore store;
    store.init_glasses({make_glass("highball"), make_glass("wine")});

    REQUIRE(store.add_active_volume(GlassId{"highball"}, ml(300.0)));

    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {300.0});
    CHECK(store.glasses().at(GlassId{"wine"}).active_volumes.empty());
}

TEST_CASE("GlassStore::remove_active_volume removes an existing volume", "[GlassStore][remove_active_volume]")
{
    const TempWorkingDir cwd;

    GlassStore store;
    store.init_glasses({make_glass("highball", {ml(200.0), ml(300.0), ml(400.0)})});

    CHECK(store.remove_active_volume(GlassId{"highball"}, ml(300.0)));
    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {200.0, 400.0});
}

TEST_CASE("GlassStore::remove_active_volume ignores a volume that is not active", "[GlassStore][remove_active_volume]")
{
    const TempWorkingDir cwd;

    GlassStore store;
    store.init_glasses({make_glass("highball", {ml(200.0)})});

    CHECK_FALSE(store.remove_active_volume(GlassId{"highball"}, ml(500.0)));
    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {200.0});
}

TEST_CASE("GlassStore::remove_active_volume ignores an unknown glass", "[GlassStore][remove_active_volume]")
{
    const TempWorkingDir cwd;

    GlassStore store;
    store.init_glasses({make_glass("highball", {ml(200.0)})});

    CHECK_FALSE(store.remove_active_volume(GlassId{"does-not-exist"}, ml(200.0)));
    check_volumes_ml(store.glasses().at(GlassId{"highball"}).active_volumes, {200.0});
}

TEST_CASE("GlassStore active volumes survive add/remove cycles", "[GlassStore][add_active_volume][remove_active_volume]")
{
    const TempWorkingDir cwd;

    GlassStore store;
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
    const TempWorkingDir cwd;

    {
        GlassStore store;
        store.init_glasses({make_glass("highball"), make_glass("wine")});
        REQUIRE(store.add_active_volume(GlassId{"highball"}, ml(400.0)));
        REQUIRE(store.add_active_volume(GlassId{"highball"}, ml(200.0)));
        REQUIRE(store.add_active_volume(GlassId{"wine"}, ml(150.0)));
    }

    GlassStore reloaded;
    reloaded.init_glasses({make_glass("highball"), make_glass("wine")});

    check_volumes_ml(reloaded.glasses().at(GlassId{"highball"}).active_volumes, {200.0, 400.0});
    check_volumes_ml(reloaded.glasses().at(GlassId{"wine"}).active_volumes, {150.0});
}

TEST_CASE("Removed active volumes are persisted", "[GlassStore][persistence]")
{
    const TempWorkingDir cwd;

    {
        GlassStore store;
        store.init_glasses({make_glass("highball", {ml(200.0), ml(300.0)})});
        REQUIRE(store.remove_active_volume(GlassId{"highball"}, ml(200.0)));
    }

    GlassStore reloaded;
    reloaded.init_glasses({make_glass("highball", {ml(200.0), ml(300.0)})});

    check_volumes_ml(reloaded.glasses().at(GlassId{"highball"}).active_volumes, {300.0});
}

TEST_CASE("save_glass_active_volumes / load_glass_active_volumes round-trip", "[glass][persistence]")
{
    const TempWorkingDir cwd;

    std::unordered_map<GlassId, Glass> saved;
    saved.emplace(GlassId{"highball"}, make_glass("highball", {ml(200.0), ml(350.5)}));
    saved.emplace(GlassId{"wine"}, make_glass("wine", {}));
    save_glass_active_volumes(saved);

    std::unordered_map<GlassId, Glass> loaded;
    loaded.emplace(GlassId{"highball"}, make_glass("highball"));
    loaded.emplace(GlassId{"wine"}, make_glass("wine", {ml(999.0)}));
    load_glass_active_volumes(loaded);

    check_volumes_ml(loaded.at(GlassId{"highball"}).active_volumes, {200.0, 350.5});
    CHECK(loaded.at(GlassId{"wine"}).active_volumes.empty());
}

TEST_CASE("load_glass_active_volumes leaves defaults when no config exists", "[glass][persistence]")
{
    const TempWorkingDir cwd;

    std::unordered_map<GlassId, Glass> glasses;
    glasses.emplace(GlassId{"highball"}, make_glass("highball", {ml(250.0)}));
    load_glass_active_volumes(glasses);

    check_volumes_ml(glasses.at(GlassId{"highball"}).active_volumes, {250.0});
}

TEST_CASE("load_glass_active_volumes sorts and de-duplicates the stored volumes", "[glass][persistence]")
{
    const TempWorkingDir cwd;
    cwd.write_file("glass_active_volumes.json",
                   R"({"version": 1, "glasses": [{"glass_id": "highball", "active_volumes": [400, 200, 400, 300]}]})");

    std::unordered_map<GlassId, Glass> glasses;
    glasses.emplace(GlassId{"highball"}, make_glass("highball"));
    load_glass_active_volumes(glasses);

    check_volumes_ml(glasses.at(GlassId{"highball"}).active_volumes, {200.0, 300.0, 400.0});
}

TEST_CASE("load_glass_active_volumes skips config entries for unknown glasses", "[glass][persistence]")
{
    const TempWorkingDir cwd;
    cwd.write_file("glass_active_volumes.json",
                   R"({"version": 1, "glasses": [{"glass_id": "gone", "active_volumes": [100]},
                       {"glass_id": "highball", "active_volumes": [300]}]})");

    std::unordered_map<GlassId, Glass> glasses;
    glasses.emplace(GlassId{"highball"}, make_glass("highball"));
    REQUIRE_NOTHROW(load_glass_active_volumes(glasses));

    CHECK(glasses.size() == 1);
    check_volumes_ml(glasses.at(GlassId{"highball"}).active_volumes, {300.0});
}

TEST_CASE("load_glass_active_volumes leaves defaults for a malformed config", "[glass][persistence]")
{
    const TempWorkingDir cwd;
    cwd.write_file("glass_active_volumes.json", R"({"version": 1, "not-glasses": [)");

    std::unordered_map<GlassId, Glass> glasses;
    glasses.emplace(GlassId{"highball"}, make_glass("highball", {ml(250.0)}));
    REQUIRE_NOTHROW(load_glass_active_volumes(glasses));

    check_volumes_ml(glasses.at(GlassId{"highball"}).active_volumes, {250.0});
}

// ── load_glasses_from_dir ─────────────────────────────────────────────────────

TEST_CASE("load_glasses_from_dir returns nothing for an empty directory", "[load_glasses_from_dir]")
{
    const TempWorkingDir cwd;
    CHECK(load_glasses_from_dir(cwd.path()).empty());
}

TEST_CASE("load_glasses_from_dir parses every field of a glass file", "[load_glasses_from_dir]")
{
    const TempWorkingDir cwd;
    cwd.write_file("highball.json", glass_json("highball", "Highball Glas", "[300, 350, 500]"));

    const auto glasses = load_glasses_from_dir(cwd.path());
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
    const TempWorkingDir cwd;
    cwd.write_file("highball.json", glass_json("highball", "Highball Glas"));

    const auto glasses = load_glasses_from_dir(cwd.path());
    REQUIRE(glasses.size() == 1);
    CHECK(glasses.front().active_volumes.empty());
}

TEST_CASE("load_glasses_from_dir treats the icon rim as optional", "[load_glasses_from_dir]")
{
    const TempWorkingDir cwd;
    cwd.write_file("willi.json", R"({
        "id": "willi",
        "display_name": "Willi Becher",
        "common_volumes_ml": [400],
        "icon": {"viewbox_width": 90, "viewbox_height": 170, "outline": "M 0 0 L 1 1"}
    })");

    const auto glasses = load_glasses_from_dir(cwd.path());
    REQUIRE(glasses.size() == 1);
    CHECK(glasses.front().icon.rim.empty());
}

TEST_CASE("load_glasses_from_dir reads all json files in the directory", "[load_glasses_from_dir]")
{
    const TempWorkingDir cwd;
    cwd.write_file("highball.json", glass_json("highball", "Highball Glas"));
    cwd.write_file("wine.json", glass_json("wine", "Weinglas"));
    cwd.write_file("margarita.json", glass_json("margarita", "Margarita Glas"));

    auto glasses = load_glasses_from_dir(cwd.path());
    REQUIRE(glasses.size() == 3);

    std::ranges::sort(glasses, {}, [](const Glass& glass) { return glass.id.raw(); });
    CHECK(glasses.at(0).id == GlassId{"highball"});
    CHECK(glasses.at(1).id == GlassId{"margarita"});
    CHECK(glasses.at(2).id == GlassId{"wine"});
}

TEST_CASE("load_glasses_from_dir ignores non-json files", "[load_glasses_from_dir]")
{
    const TempWorkingDir cwd;
    cwd.write_file("highball.json", glass_json("highball", "Highball Glas"));
    cwd.write_file("README.md", "not a glass");
    cwd.write_file("wine.json.bak", glass_json("wine", "Weinglas"));

    const auto glasses = load_glasses_from_dir(cwd.path());
    REQUIRE(glasses.size() == 1);
    CHECK(glasses.front().id == GlassId{"highball"});
}

TEST_CASE("load_glasses_from_dir ignores subdirectories", "[load_glasses_from_dir]")
{
    const TempWorkingDir cwd;
    cwd.write_file("highball.json", glass_json("highball", "Highball Glas"));
    std::filesystem::create_directory(cwd.path() / "nested.json");

    const auto glasses = load_glasses_from_dir(cwd.path());
    REQUIRE(glasses.size() == 1);
    CHECK(glasses.front().id == GlassId{"highball"});
}

TEST_CASE("load_glasses_from_dir throws on a malformed glass file", "[load_glasses_from_dir]")
{
    const TempWorkingDir cwd;
    cwd.write_file("broken.json", R"({"id": "broken", "display_name": )");

    CHECK_THROWS_AS(load_glasses_from_dir(cwd.path()), std::runtime_error);
}

TEST_CASE("load_glasses_from_dir throws when a required field is missing", "[load_glasses_from_dir]")
{
    const TempWorkingDir cwd;
    cwd.write_file("incomplete.json", R"({"id": "incomplete", "display_name": "No Icon", "common_volumes_ml": [200]})");

    CHECK_THROWS_AS(load_glasses_from_dir(cwd.path()), std::runtime_error);
}

TEST_CASE("load_glasses_from_dir throws for a non-existing directory", "[load_glasses_from_dir]")
{
    const TempWorkingDir cwd;
    CHECK_THROWS_AS(load_glasses_from_dir(cwd.path() / "does-not-exist"), std::filesystem::filesystem_error);
}
