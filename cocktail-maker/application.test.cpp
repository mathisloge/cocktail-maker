#include <catch2/catch_test_macros.hpp>
#include <stdexec/execution.hpp>
import std;
import cm.core;
import cm.testing;
import cm;

using namespace std::chrono_literals;

namespace {
class TestApplication : public cm::Application
{
  public:
    using Application::Application;
    using Application::run;

    const cm::GlassStore& glasses() const
    {
        return glass_store_;
    }

    const cm::StationConfig& config() const
    {
        return station_config_;
    }
};

class NoPodsStationState : public cm::StationState
{
  public:
    std::unique_ptr<cm::PodState> create_pod_state() override
    {
        return nullptr;
    }
};

/// Never discovers anything and waits until it is stopped.
class WaitingPodDiscovery : public cm::PodDiscovery
{
  public:
    explicit WaitingPodDiscovery(std::atomic<bool>& stopped)
        : stopped_{stopped}
    {
    }

    cm::Task<void> discover(cm::AsyncScope& scope, std::function<void(std::shared_ptr<cm::IPod>)> /*on_pod_discovered*/) override
    {
        co_await (cm::wait_until_stopped(scope.executor()) | stdexec::upon_stopped([this]() noexcept { stopped_ = true; }));
    }

  private:
    std::atomic<bool>& stopped_;
};
} // namespace

TEST_CASE("Application - shutdown stops and joins all asynchronous work", "[application]")
{
    std::atomic<bool> discovery_stopped{false};
    cm::testing::TempDir dir;
    TestApplication app{dir.path()};
    app.run(std::make_shared<NoPodsStationState>(), std::make_unique<WaitingPodDiscovery>(discovery_stopped));

    app.shutdown();
    REQUIRE(discovery_stopped);

    // A second shutdown is a no-op.
    app.shutdown();
}

TEST_CASE("Application - shutdown without run is a no-op", "[application]")
{
    cm::testing::TempDir dir;
    TestApplication app{dir.path()};
    app.shutdown();
}

TEST_CASE("The application reads its state from the directory it was given", "[application]")
{
    cm::testing::TempDir dir;
    for (const auto* catalog : {"glasses", "ingredients", "recipes"}) {
        std::filesystem::create_directories(dir.path() / "db" / catalog);
    }
    dir.write_file("db/glasses/highball.json", R"({"id": "highball", "display_name": "Highball", "common_volumes_ml": [300],
        "icon": {"viewbox_width": 100, "viewbox_height": 180, "outline": "M 0 0"}})");
    dir.write_file("db/ingredients/rum.json", R"({"id": "rum", "display_name": "Rum", "type": "alcohol", "boost": "boostable"})");
    dir.write_file("glass_active_volumes.json",
                   R"({"version": 1, "glasses": [{"glass_id": "highball", "active_volumes": [300]}]})");
    dir.write_file("ingredient_dispenser_mapping.json",
                   R"({"version": 1, "mappings": [{"ingredient_id": "rum", "pod_id": "Pod", "dispenser_id": 0}]})");
    dir.write_file("dispenser_fill_levels.json",
                   R"({"version": 1, "fill_levels": [{"pod_id": "Pod", "dispenser_id": 0, "level_g": 250,
                       "low_water_g": 0, "suspected_empty": false}]})");

    TestApplication app{dir.path()};
    app.init(dir.path() / "db");

    const cm::PodDispenser address{cm::PodId{"Pod"}, cm::DispenserId{0}};
    CHECK(app.fill_levels().record(address).has_value());
    CHECK(app.glasses().glasses().at(cm::GlassId{"highball"}).active_volumes.size() == 1);
    const auto assigned = app.config().find_dispenser_for_ingredient(cm::IngredientId{"rum"});
    REQUIRE(assigned.has_value());
    CHECK(*assigned == address);
}

TEST_CASE("A broken catalog file keeps the stored fill levels", "[application]")
{
    cm::testing::TempDir dir;
    for (const auto* catalog : {"glasses", "ingredients", "recipes"}) {
        std::filesystem::create_directories(dir.path() / "db" / catalog);
    }
    dir.write_file("db/recipes/broken.json", R"({"name": "Broken", "steps": []})");
    dir.write_file("dispenser_fill_levels.json",
                   R"({"version": 1, "fill_levels": [{"pod_id": "Pod", "dispenser_id": 0, "level_g": 250,
                       "low_water_g": 0, "suspected_empty": false}]})");

    TestApplication app{dir.path()};
    REQUIRE_THROWS(app.init(dir.path() / "db"));

    CHECK(app.fill_levels().record(cm::PodDispenser{cm::PodId{"Pod"}, cm::DispenserId{0}}).has_value());
}
