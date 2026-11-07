#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <exec/asio/use_sender.hpp>
#include <exec/when_any.hpp>
#include <proto/field/ErrorCodeCommon.h>
#include <stdexec/execution.hpp>

import std;
import mp_units;
import cm.core;
import cm.testing;
import cm;

#include "fake_dispenser.test.hpp"

using namespace std::chrono_literals;
namespace units = cm::units;

namespace {
units::Grams grams(double value)
{
    return value * units::si::gram;
}

const cm::PodDispenser kAddress{cm::PodId{"Pod"}, cm::DispenserId{0}};

struct FillLevelFixture
{
    boost::asio::io_context ctx;
    cm::testing::TempDir dir{"cm-fill-level-test"};
    std::filesystem::path file = dir.file("levels.json");
    cm::FillLevelOptions options{.refill_settle_time = 1ms};
    std::unique_ptr<cm::FillLevelTracker> tracker = std::make_unique<cm::FillLevelTracker>(ctx.get_executor(), file, options);
    FakeDispenser dispenser;

    ~FillLevelFixture()
    {
        // The tracker's writer has to finish before the directory goes away.
        tracker.reset();
    }

    void run(cm::Task<void> task)
    {
        REQUIRE(cm::run_until_complete(ctx, std::move(task)));
    }

    void refresh(double reading)
    {
        dispenser.reading = grams(reading);
        run(tracker->refresh(kAddress, dispenser));
    }

    void connect(double reading)
    {
        dispenser.reading = grams(reading);
        run(tracker->correct_on_connect(kAddress, dispenser));
    }

    void refilled(double reading, cm::RefillSource source = cm::RefillSource::mix_dialog)
    {
        dispenser.reading = grams(reading);
        run(tracker->refilled(kAddress, dispenser, source));
    }

    void pour_finished(double amount, cm::DispenseOutcome outcome, double reading_after)
    {
        dispenser.reading = grams(reading_after);
        run(tracker->pour_finished(
            kAddress, dispenser, cm::DispenseReport{.amount = amount * units::milli_litre, .outcome = outcome}));
    }

    /// An empty bottle was tared and filled to `level`.
    void start_at(double level)
    {
        tracker->empty_bottle_tared(kAddress);
        refresh(level);
    }

    /// A bottle was filled to `capacity` and has since gone down to `level`.
    void start_at(double capacity, double level)
    {
        start_at(capacity);
        refilled(capacity);
        refresh(level);
    }

    cm::FillLevelRecord record() const
    {
        const auto found = tracker->record(kAddress);
        REQUIRE(found.has_value());
        return *found;
    }

    double level() const
    {
        return record().level.numerical_value_in(units::si::gram);
    }

    std::string file_content()
    {
        tracker->flush();
        std::ifstream in{file};
        return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    }
};
} // namespace

TEST_CASE_METHOD(FillLevelFixture, "A power loss keeps the level and only moves the zero point", "[fill_level]")
{
    start_at(640);
    tracker->pod_disconnected(kAddress.pod_id);

    connect(0);
    CHECK(level() == 640);

    refresh(-100);
    CHECK(level() == 540);
}

TEST_CASE_METHOD(FillLevelFixture, "A reconnect after an earlier power loss keeps the correction", "[fill_level]")
{
    start_at(640);
    tracker->pod_disconnected(kAddress.pod_id);
    connect(0);
    refresh(-100);

    tracker->pod_disconnected(kAddress.pod_id);
    connect(-100);
    CHECK(level() == 540);

    refresh(-150);
    CHECK(level() == 490);
}

TEST_CASE_METHOD(FillLevelFixture, "A failed read at connect is made up for at the next read", "[fill_level]")
{
    start_at(640);
    tracker->pod_disconnected(kAddress.pod_id);

    dispenser.no_load_cell = true;
    connect(0);
    CHECK(level() == 640);

    dispenser.no_load_cell = false;
    refresh(-100);
    CHECK(level() == 640);

    refresh(-130);
    CHECK(level() == 610);
}

TEST_CASE_METHOD(FillLevelFixture, "A pod that is gone leaves the level alone", "[fill_level]")
{
    start_at(640);
    dispenser.pod_gone = true;
    refresh(100);
    CHECK(level() == 640);
}

TEST_CASE_METHOD(FillLevelFixture, "Connecting a dispenser without a record keeps the level unknown", "[fill_level]")
{
    std::vector<std::pair<cm::PodDispenser, cm::FillLevelView>> changes;
    tracker->set_change_handler(
        [&changes](const cm::PodDispenser& address, const cm::FillLevelView& view) { changes.emplace_back(address, view); });

    connect(100);

    CHECK_FALSE(tracker->record(kAddress).has_value());
    REQUIRE(changes.size() == 1);
    CHECK(changes.front().first == kAddress);
    CHECK_FALSE(changes.front().second.level.has_value());
}

TEST_CASE_METHOD(FillLevelFixture, "A dispenser without a load cell says so after the read at connect", "[fill_level]")
{
    dispenser.no_load_cell = true;
    connect(0);

    CHECK(tracker->view(kAddress).no_load_cell);
    CHECK_FALSE(tracker->view(kAddress).level.has_value());
    CHECK(dispenser.weight_reads == 1);
}

TEST_CASE_METHOD(FillLevelFixture, "A failed read after a pour subtracts the reported amount", "[fill_level]")
{
    start_at(100);
    dispenser.no_load_cell = true;
    pour_finished(30, cm::DispenseOutcome::completed, 70);
    CHECK(level() == 70);
}

TEST_CASE_METHOD(FillLevelFixture, "A short pour only makes the bottle suspected empty", "[fill_level]")
{
    start_at(30);
    pour_finished(30, cm::DispenseOutcome::bottle_empty, 0);

    CHECK(level() == 0);
    CHECK(record().suspected_empty);
    CHECK(tracker->view(kAddress).suspected_empty);
}

TEST_CASE_METHOD(FillLevelFixture, "A blocked hose clears itself with the next pour", "[fill_level]")
{
    start_at(1000, 800);
    pour_finished(0, cm::DispenseOutcome::bottle_empty, 800);
    REQUIRE(record().suspected_empty);

    pour_finished(40, cm::DispenseOutcome::completed, 760);

    CHECK_FALSE(record().suspected_empty);
    CHECK(level() == 760);
    CHECK(record().low_water == grams(760));
    CHECK(record().capacity == grams(1000));
}

TEST_CASE_METHOD(FillLevelFixture, "A bottle off the scale doesn't count towards the next refill", "[fill_level]")
{
    start_at(1000, 30);
    pour_finished(30, cm::DispenseOutcome::bottle_empty, 0);

    // The bottle is off the scale while somebody confirms the dialog, and the next pour comes out dry.
    refilled(-500);
    pour_finished(0, cm::DispenseOutcome::bottle_empty, -500);

    refilled(970);

    CHECK(level() == 970);
    CHECK(record().capacity == grams(970));
}

TEST_CASE_METHOD(FillLevelFixture, "Another empty pour that took something counts from the new low", "[fill_level]")
{
    start_at(1000, 800);
    pour_finished(0, cm::DispenseOutcome::bottle_empty, 800);

    pour_finished(100, cm::DispenseOutcome::bottle_empty, 700);
    CHECK(record().low_water == grams(700));

    refilled(1000);
    CHECK(level() == 300);
}

TEST_CASE_METHOD(FillLevelFixture, "A refill after an empty bottle takes the amount that went in", "[fill_level]")
{
    // The level is 30 g too high here, as if the bottle had drifted or been miscounted.
    start_at(1000, 30);
    pour_finished(0, cm::DispenseOutcome::bottle_empty, 30);

    refilled(1000);

    CHECK(level() == 970);
    CHECK(record().capacity == grams(970));
    CHECK(record().low_water == grams(970));
    CHECK_FALSE(record().suspected_empty);

    refresh(900);
    CHECK(level() == 870);
}

TEST_CASE_METHOD(FillLevelFixture, "Confirming the dialog without a refill changes nothing", "[fill_level]")
{
    start_at(1000, 800);
    pour_finished(0, cm::DispenseOutcome::bottle_empty, 800);
    const auto before = record();

    refilled(800);

    CHECK(record() == before);
}

TEST_CASE_METHOD(FillLevelFixture, "A refill has to rise by the minimum to count", "[fill_level]")
{
    start_at(0);

    refilled(49);
    CHECK_FALSE(record().capacity.has_value());

    refilled(50);
    CHECK(record().capacity == grams(50));
}

TEST_CASE_METHOD(FillLevelFixture, "A top-up read before the button still counts as a refill", "[fill_level]")
{
    start_at(1000, 300);

    refresh(800);
    refilled(800);

    CHECK(level() == 800);
    CHECK(record().capacity == grams(800));
}

TEST_CASE_METHOD(FillLevelFixture, "A top-up above the capacity raises it", "[fill_level]")
{
    start_at(1000, 1000);

    refresh(1100);

    CHECK(record().capacity == grams(1100));
    CHECK(tracker->view(kAddress).percent == 100.0 * units::percent);
}

TEST_CASE_METHOD(FillLevelFixture, "The admin button without a rise means the bottle is full", "[fill_level]")
{
    start_at(1000, 300);

    refilled(300, cm::RefillSource::admin_area);
    CHECK(level() == 1000);

    refresh(200);
    CHECK(level() == 900);
}

TEST_CASE_METHOD(FillLevelFixture, "The admin button without a rise or a capacity changes nothing", "[fill_level]")
{
    start_at(30);
    const auto before = record();

    refilled(30, cm::RefillSource::admin_area);

    CHECK(record() == before);
}

TEST_CASE_METHOD(FillLevelFixture, "Commissioning counts a calibration run as part of the filled amount", "[fill_level]")
{
    tracker->empty_bottle_tared(kAddress);
    refresh(950);
    refresh(900);

    refilled(900);

    CHECK(level() == 900);
    CHECK(record().capacity == grams(900));
    CHECK(tracker->view(kAddress).percent == 100.0 * units::percent);
}

TEST_CASE_METHOD(FillLevelFixture, "Taring the empty scale forgets the level", "[fill_level]")
{
    start_at(1000, 300);

    tracker->clear_scale_tared(kAddress);

    CHECK_FALSE(tracker->record(kAddress).has_value());
    CHECK_FALSE(tracker->view(kAddress).level.has_value());
}

TEST_CASE_METHOD(FillLevelFixture, "The view never shows a level below zero", "[fill_level]")
{
    start_at(10);
    refresh(-5);

    CHECK(tracker->view(kAddress).level == grams(0));
}

TEST_CASE_METHOD(FillLevelFixture, "A stop during the wait after the button leaves the level alone", "[fill_level]")
{
    start_at(1000, 300);
    const auto before = record();

    auto slow = cm::FillLevelTracker{ctx.get_executor(), file, cm::FillLevelOptions{.refill_settle_time = 10s}};
    slow.empty_bottle_tared(kAddress);
    const auto slow_before = slow.record(kAddress);

    auto stop_soon = [this]() -> cm::Task<void> { co_await cm::async_sleep(ctx.get_executor(), 10ms); };
    dispenser.reading = grams(900);
    run([&]() -> cm::Task<void> {
        co_await exec::when_any(slow.refilled(kAddress, dispenser, cm::RefillSource::admin_area), stop_soon());
    }());

    CHECK(slow.record(kAddress) == slow_before);
    CHECK(record() == before);
}

TEST_CASE_METHOD(FillLevelFixture, "The records survive a restart", "[fill_level]")
{
    start_at(1000, 300);
    pour_finished(0, cm::DispenseOutcome::bottle_empty, 300);
    const auto before = record();

    tracker->flush();
    tracker = std::make_unique<cm::FillLevelTracker>(ctx.get_executor(), file, options);
    tracker->init();

    CHECK(record() == before);
}

TEST_CASE_METHOD(FillLevelFixture, "A file full of garbage starts empty", "[fill_level]")
{
    {
        std::ofstream out{file};
        out << "not json at all";
    }

    tracker->init();

    CHECK_FALSE(tracker->record(kAddress).has_value());
}

TEST_CASE_METHOD(FillLevelFixture, "A malformed entry doesn't cost the others", "[fill_level]")
{
    {
        std::ofstream out{file};
        out << R"({"version":1,"fill_levels":[
            {"pod_id":"Pod","dispenser_id":7,"capacity_g":10},
            {"pod_id":"Pod","dispenser_id":0,"level_g":640,"low_water_g":640,"suspected_empty":false}
        ]})";
    }

    tracker->init();

    CHECK(level() == 640);
    CHECK_FALSE(record().capacity.has_value());
    CHECK_FALSE(tracker->record(cm::PodDispenser{cm::PodId{"Pod"}, cm::DispenserId{7}}).has_value());
}

TEST_CASE("A file that can't be written only logs", "[fill_level]")
{
    boost::asio::io_context ctx;
    cm::FillLevelTracker tracker{ctx.get_executor(),
                                 std::filesystem::temp_directory_path() / "missing-directory" / "levels.json"};

    tracker.empty_bottle_tared(kAddress);
    tracker.flush();

    CHECK(tracker.record(kAddress).has_value());
}

TEST_CASE_METHOD(FillLevelFixture, "Saving leaves one complete file behind", "[fill_level]")
{
    start_at(1000, 640);

    const auto content = file_content();

    CHECK(content.contains(R"("version")"));
    CHECK(content.contains(R"("pod_id")"));
    CHECK(content.contains(R"("dispenser_id")"));
    CHECK(content.contains(R"("level_g")"));
    CHECK(content.contains(R"("capacity_g")"));
    CHECK(content.contains(R"("low_water_g")"));
    CHECK(content.contains(R"("suspected_empty")"));
    CHECK_FALSE(std::filesystem::exists(std::filesystem::path{file} += ".tmp"));
}

namespace {
/// A catalog with two ingredients assigned to two dispensers.
struct ShortageFixture : FillLevelFixture
{
    cm::IngredientStore ingredients;
    cm::StationConfig config{ingredients, ""};
    const cm::IngredientId rum{"rum"};
    const cm::IngredientId lime{"lime"};
    const cm::PodDispenser lime_address{cm::PodId{"Pod"}, cm::DispenserId{1}};

    ShortageFixture()
    {
        ingredients.init_ingredients({
            cm::Ingredient{.id = rum, .display_name = "Rum", .type = {}, .boost_category = cm::BoostCategory::fixed},
            cm::Ingredient{.id = lime, .display_name = "Limette", .type = {}, .boost_category = cm::BoostCategory::fixed},
        });
        config.update_dispenser_ingredient_mapping(rum, kAddress);
        config.update_dispenser_ingredient_mapping(lime, lime_address);
    }

    cm::Command pour(const cm::IngredientId& ingredient, double millilitres)
    {
        return cm::DispenseCommand{cm::CommandId{1}, ingredient, millilitres * units::milli_litre};
    }

    std::vector<cm::FillShortage> shortages(const cm::Commands& commands)
    {
        return cm::find_fill_shortages(commands, config, *tracker);
    }
};
} // namespace

TEST_CASE_METHOD(ShortageFixture, "An amount that fits with the reserve isn't short", "[fill_level][shortage]")
{
    start_at(60);
    CHECK(shortages({pour(rum, 40)}).empty());
}

TEST_CASE_METHOD(ShortageFixture, "An amount that doesn't fit with the reserve is short", "[fill_level][shortage]")
{
    start_at(59);

    const auto found = shortages({pour(rum, 40)});

    REQUIRE(found.size() == 1);
    CHECK(found.front().ingredient == rum);
    CHECK(std::lround(found.front().available.numerical_value_in(units::milli_litre)) == 59);
    CHECK(std::lround(found.front().needed.numerical_value_in(units::milli_litre)) == 40);
}

TEST_CASE_METHOD(ShortageFixture, "Several steps of one ingredient add up", "[fill_level][shortage]")
{
    start_at(70);

    const auto found = shortages({pour(rum, 30), cm::ParallelCommand{pour(rum, 30), pour(lime, 10)}});

    REQUIRE(found.size() == 1);
    CHECK(std::lround(found.front().needed.numerical_value_in(units::milli_litre)) == 60);
}

TEST_CASE_METHOD(ShortageFixture, "A suspected empty bottle counts as empty", "[fill_level][shortage]")
{
    start_at(1000, 800);
    pour_finished(0, cm::DispenseOutcome::bottle_empty, 800);

    const auto found = shortages({pour(rum, 40)});

    REQUIRE(found.size() == 1);
    CHECK(found.front().available == 0.0 * units::milli_litre);
}

TEST_CASE_METHOD(ShortageFixture, "An unknown level or a missing assignment isn't short", "[fill_level][shortage]")
{
    CHECK(shortages({pour(rum, 40)}).empty());
    CHECK(shortages({pour(cm::IngredientId{"unassigned"}, 40)}).empty());
}

TEST_CASE_METHOD(ShortageFixture, "The dispensers of a recipe come once each", "[fill_level][shortage]")
{
    const auto found = cm::involved_dispensers({pour(rum, 30), cm::ParallelCommand{pour(rum, 30), pour(lime, 10)}}, config);

    CHECK(found == std::vector<cm::PodDispenser>{kAddress, lime_address});
}
