#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <proto/field/ErrorCodeCommon.h>
#include <slint.h>
#include "app-window.h"

import std;
import mp_units;
import cm.core;
import cm.testing;
import cm;
import cm.gui;

#include "fake_pod.test.hpp"
#include "slint_events.test.hpp"

TEST_CASE("StationStateBridge - Initialization", "[gui][StationStateBridge]")
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    auto ui = cm::gui::AppWindow::create();
    cm::PodRegistry pod_registry;
    cm::FillLevelTracker fill_levels{ctx.get_executor(), "unused_fill_levels.json"};
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui, pod_registry, scope, fill_levels);
    SlintFlusher flusher;

    SECTION("Pod UI model is created and initially empty")
    {
        auto model = bridge->pod_model();
        REQUIRE(model != nullptr);
        REQUIRE(model->row_count() == 0);

        // Out-of-bounds queries should safely return nullopt
        REQUIRE(model->row_data(0) == std::nullopt);
    }
}

TEST_CASE("StationStateBridge - Pod Lifecycle", "[gui][StationStateBridge]")
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    auto ui = cm::gui::AppWindow::create();
    cm::PodRegistry pod_registry;
    cm::FillLevelTracker fill_levels{ctx.get_executor(), "unused_fill_levels.json"};
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui, pod_registry, scope, fill_levels);
    auto model = bridge->pod_model();
    SlintFlusher flusher;

    SECTION("Creating a pod defers model addition during the Discovery Phase (empty ID)")
    {
        auto pod = bridge->create_pod_state();
        REQUIRE(pod != nullptr);

        flush_slint_events();

        // ID is empty/default, it should NOT be inserted into the UI model yet
        REQUIRE(model->row_count() == 0);
    }

    SECTION("Assigning a valid ID to a pod adds it to the model")
    {
        auto pod = bridge->create_pod_state();

        pod->update_info({cm::PodId{"pod-123"}});
        flush_slint_events();

        REQUIRE(model->row_count() == 1);
        auto pod_data = model->row_data(0);
        REQUIRE(pod_data.has_value());
        REQUIRE(pod_data->id == slint::SharedString{"pod-123"});
        REQUIRE(pod_data->connection_state == cm::gui::ConnectionState::Disconnected);
    }

    SECTION("State updates map exactly to UI ConnectionState enums")
    {
        auto pod = bridge->create_pod_state();
        pod->update_info({cm::PodId{"pod-states"}});
        flush_slint_events();

        REQUIRE(model->row_data(0)->connection_state == cm::gui::ConnectionState::Disconnected);

        // Transition to Connecting
        pod->update_state(cm::PodState::ConnectionState::connecting);
        flush_slint_events();
        REQUIRE(model->row_data(0)->connection_state == cm::gui::ConnectionState::Connecting);

        // Transition to Connected
        pod->update_state(cm::PodState::ConnectionState::connected);
        flush_slint_events();
        REQUIRE(model->row_data(0)->connection_state == cm::gui::ConnectionState::Connected);

        // Transition back to Unknown (maps downwards to Disconnected in internal switch)
        pod->update_state(cm::PodState::ConnectionState::unknown);
        flush_slint_events();
        REQUIRE(model->row_data(0)->connection_state == cm::gui::ConnectionState::Disconnected);
    }

    SECTION("State updates before an ID assignment are deferred correctly")
    {
        auto pod = bridge->create_pod_state();

        // Update connection state while ID is still unassigned
        pod->update_state(cm::PodState::ConnectionState::connected);
        flush_slint_events();

        // Verify it was dropped/deferred and not yet added to the model
        REQUIRE(model->row_count() == 0);

        // Setting the ID triggers processing and applies the previously cached state
        pod->update_info({cm::PodId{"pod-deferred"}});
        flush_slint_events();

        REQUIRE(model->row_count() == 1);
        REQUIRE(model->row_data(0)->connection_state == cm::gui::ConnectionState::Connected);
    }

    SECTION("A connected pod lists labelled dispensers and its firmware")
    {
        auto pod = bridge->create_pod_state();
        pod->update_info({.id = cm::PodId{"pod-labelled"}, .firmware_version = {1, 4, 0}, .num_pumps = 2, .num_valves = 1});
        pod->update_state(cm::PodState::ConnectionState::connected);
        flush_slint_events();

        REQUIRE(model->row_count() == 1);
        const auto row = model->row_data(0).value();
        CHECK(row.firmware_version == slint::SharedString{"1.4.0"});
        CHECK(row.pump_count == 2);
        CHECK(row.valve_count == 1);
        REQUIRE(row.dispensers->row_count() == 3);
        const std::array<std::string, 3> labels{"Pumpe 1", "Pumpe 2", "Ventil 1"};
        for (std::size_t i = 0; i < labels.size(); ++i) {
            const auto dispenser = row.dispensers->row_data(i).value();
            CHECK(std::string{dispenser.label.data()} == labels[i]);
            CHECK(std::string{dispenser.fill_text.data()} == "Füllstand unbekannt");
            CHECK_FALSE(dispenser.fill_known);
        }
    }
}

TEST_CASE("StationStateBridge - Dispensers show the drink assigned to them", "[gui][StationStateBridge]")
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    auto ui = cm::gui::AppWindow::create();
    cm::PodRegistry pod_registry;
    cm::testing::TempDir dir;
    cm::FillLevelTracker fill_levels{ctx.get_executor(), dir.file("fill_levels.json")};
    cm::IngredientStore ingredients;
    ingredients.init_ingredients({
        cm::Ingredient{.id = cm::IngredientId{"rum"}, .display_name = "Weißer Rum", .type = {}},
        cm::Ingredient{.id = cm::IngredientId{"lime"}, .display_name = "Limettensaft", .type = {}},
    });
    cm::StationConfig station_config{ingredients, dir.file("mapping.json")};
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui, pod_registry, scope, fill_levels);
    auto model = bridge->pod_model();
    SlintFlusher flusher;

    auto pod = bridge->create_pod_state();
    pod->update_info({.id = cm::PodId{"Pod-A"}, .num_pumps = 2, .num_valves = 0});
    pod->update_state(cm::PodState::ConnectionState::connected);
    bridge->watch_assignments(station_config, ingredients);
    flush_slint_events();

    const auto name_of = [&model](std::size_t dispenser) {
        return std::string{model->row_data(0)->dispensers->row_data(dispenser)->ingredient_name.data()};
    };
    REQUIRE(model->row_count() == 1);

    station_config.update_dispenser_ingredient_mapping(cm::IngredientId{"rum"},
                                                       cm::PodDispenser{cm::PodId{"Pod-A"}, cm::DispenserId{0}});
    flush_slint_events();
    CHECK(name_of(0) == "Weißer Rum");
    CHECK(name_of(1) == "");

    station_config.update_dispenser_ingredient_mapping(cm::IngredientId{"rum"},
                                                       cm::PodDispenser{cm::PodId{"Pod-A"}, cm::DispenserId{1}});
    flush_slint_events();
    CHECK(name_of(0) == "");
    CHECK(name_of(1) == "Weißer Rum");
    station_config.flush();
}

TEST_CASE("StationStateBridge - Readiness Status and Multiple Pods", "[gui][StationStateBridge]")
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    auto ui = cm::gui::AppWindow::create();
    cm::PodRegistry pod_registry;
    cm::FillLevelTracker fill_levels{ctx.get_executor(), "unused_fill_levels.json"};
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui, pod_registry, scope, fill_levels);
    auto model = bridge->pod_model();

    SlintFlusher flusher;

    SECTION("Device readiness becomes true only when ALL pods are fully connected")
    {
        auto pod1 = bridge->create_pod_state();
        auto pod2 = bridge->create_pod_state();

        pod1->update_info({cm::PodId{"pod-1"}});
        pod2->update_info({cm::PodId{"pod-2"}});
        flush_slint_events();

        REQUIRE(model->row_count() == 2);

        // Connect pod 1
        pod1->update_state(cm::PodState::ConnectionState::connected);
        flush_slint_events();

        REQUIRE(model->row_data(0)->connection_state == cm::gui::ConnectionState::Connected);
        REQUIRE(model->row_data(1)->connection_state == cm::gui::ConnectionState::Disconnected);

        // Connect pod 2
        pod2->update_state(cm::PodState::ConnectionState::connected);
        flush_slint_events();

        REQUIRE(model->row_data(1)->connection_state == cm::gui::ConnectionState::Connected);
    }

    SECTION("Pod destruction explicitly removes it from the UI model")
    {
        {
            auto pod = bridge->create_pod_state();
            pod->update_info({.id = cm::PodId{"pod-to-remove"}});
            pod->update_state(cm::PodState::ConnectionState::connected);
            flush_slint_events();

            REQUIRE(model->row_count() == 1);
            REQUIRE(model->row_data(0)->id == slint::SharedString{"pod-to-remove"});
        }
        // `pod` explicitly went out of scope and gets deleted here.

        flush_slint_events();

        // The UI Model should now reflect the pod's removal
        REQUIRE(model->row_count() == 0);
        REQUIRE(model->row_data(0) == std::nullopt);
    }
}

TEST_CASE("present_fill_level words and measures what is known about a bottle", "[gui][StationStateBridge]")
{
    const auto grams = [](double value) { return value * cm::units::si::gram; };
    using cm::units::percent;
    const auto unknown = cm::gui::present_fill_level(cm::FillLevelView{});
    CHECK(unknown.text == "Füllstand unbekannt");
    CHECK_FALSE(unknown.known);
    CHECK_FALSE(unknown.almost_empty);
    CHECK(cm::gui::present_fill_level(cm::FillLevelView{.no_load_cell = true}).text == "Füllstand unbekannt");
    const auto no_capacity = cm::gui::present_fill_level(cm::FillLevelView{.level = grams(640)});
    CHECK(no_capacity.text == "ca. 640 mL");
    CHECK_FALSE(no_capacity.known);
    const auto full = cm::gui::present_fill_level(cm::FillLevelView{.level = grams(640), .percent = 64.0 * percent});
    CHECK(full.text == "640 mL von 1000 mL");
    CHECK(full.known);
    CHECK(full.ratio == Catch::Approx(0.64));
    CHECK_FALSE(full.almost_empty);
    CHECK(cm::gui::present_fill_level(cm::FillLevelView{.level = grams(90), .percent = 14.0 * percent}).almost_empty);
    CHECK_FALSE(cm::gui::present_fill_level(cm::FillLevelView{.level = grams(150), .percent = 15.0 * percent}).almost_empty);
    const auto drained = cm::gui::present_fill_level(cm::FillLevelView{.level = grams(0), .percent = 0.0 * percent});
    CHECK(drained.text == "0 mL");
    CHECK(drained.known);
    CHECK(drained.almost_empty);
    const auto empty =
        cm::gui::present_fill_level(cm::FillLevelView{.level = grams(30), .percent = 3.0 * percent, .suspected_empty = true});
    CHECK(empty.text == "vermutlich leer");
    CHECK(empty.known);
    CHECK(empty.almost_empty);
    CHECK(empty.ratio == 0.0F);
}

TEST_CASE("dispenser_label numbers pumps and valves from one", "[gui][StationStateBridge]")
{
    CHECK(cm::gui::dispenser_label(cm::gui::DispenserType::Pump, 1) == "Pumpe 1");
    CHECK(cm::gui::dispenser_label(cm::gui::DispenserType::Valve, 2) == "Ventil 2");
}

TEST_CASE("StationStateBridge - A pod on another protocol release shows why it isn't used", "[gui][StationStateBridge]")
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    auto ui = cm::gui::AppWindow::create();
    cm::PodRegistry pod_registry;
    cm::FillLevelTracker fill_levels{ctx.get_executor(), "unused_fill_levels.json"};
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui, pod_registry, scope, fill_levels);
    SlintFlusher flusher;

    auto pod = bridge->create_pod_state();
    pod->update_info(cm::PodInfo{.id = cm::PodId{"Newer"},
                                 .firmware_version = {},
                                 .protocol_version = cm::Version{.major = 2, .minor = 2, .patch = 0},
                                 .num_pumps = 1,
                                 .num_valves = 0});
    pod->update_state(cm::PodState::ConnectionState::incompatible);
    flush_slint_events();

    const auto row = bridge->pod_model()->row_data(0);
    REQUIRE(row.has_value());
    CHECK(row->connection_state == cm::gui::ConnectionState::Incompatible);
    CHECK(std::string{row->status_text.data()} == "Protokoll 2.2.0, erwartet 2.1.0");
    REQUIRE(row->dispensers != nullptr);
    CHECK(row->dispensers->row_count() == 0);
    CHECK_FALSE(ui->global<cm::gui::StationStateContext>().get_station_ready());

    auto usable = bridge->create_pod_state();
    usable->update_info(cm::PodInfo{.id = cm::PodId{"Current"},
                                    .firmware_version = {},
                                    .protocol_version = cm::station_protocol_version(),
                                    .num_pumps = 1,
                                    .num_valves = 0});
    usable->update_state(cm::PodState::ConnectionState::connected);
    flush_slint_events();

    // The unused Pod doesn't hold back the one that works, just like an unplugged one.
    CHECK(ui->global<cm::gui::StationStateContext>().get_station_ready());

    usable.reset();
    pod.reset();
}

TEST_CASE("StationStateBridge - An operator lockout counts down on the UI thread alone", "[gui][StationStateBridge][operator]")
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    auto ui = cm::gui::AppWindow::create();
    cm::PodRegistry pod_registry;
    cm::FillLevelTracker fill_levels{ctx.get_executor(), "unused_fill_levels.json"};
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui, pod_registry, scope, fill_levels);
    SlintFlusher flusher;
    const auto& context = ui->global<cm::gui::StationStateContext>();

    for (int attempt = 0; attempt < 4; ++attempt) {
        context.invoke_submit_operator_pin(slint::SharedString{"111111"});
    }

    CHECK(context.get_operator_locked_out());
    CHECK(context.get_operator_remaining_attempts() == 0);
    CHECK(context.get_operator_lockout_seconds_remaining() > 0);
    // OperatorAuth isn't shared with the execution context, so nothing of the lockout may run there.
    CHECK(ctx.poll() == 0);

    // A scope has to be joined before it goes away, also when a failed check above left work in it.
    cm::run_until_complete(ctx, scope.stop_and_join());
}

TEST_CASE("StationStateBridge - Pod problems", "[gui][StationStateBridge]")
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    auto ui = cm::gui::AppWindow::create();
    cm::PodRegistry pod_registry;
    cm::testing::TempDir dir;
    cm::FillLevelTracker fill_levels{ctx.get_executor(), dir.file("fill_levels.json")};
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui, pod_registry, scope, fill_levels);
    const auto& context = ui->global<cm::gui::StationStateContext>();
    SlintFlusher flusher;

    const auto problems = [&context]() {
        std::vector<cm::gui::PodProblem> rows;
        const auto model = context.get_pod_problems();
        for (std::size_t row = 0; model != nullptr and row < model->row_count(); ++row) {
            rows.push_back(model->row_data(row).value());
        }
        return rows;
    };
    const auto connect_pod_a = [&bridge]() {
        auto pod = bridge->create_pod_state();
        pod->update_info({.id = cm::PodId{"Pod-A"}, .num_pumps = 1, .num_valves = 0});
        pod->update_state(cm::PodState::ConnectionState::connected);
        return pod;
    };

    SECTION("A pod that goes away leaves one problem until it connects again")
    {
        auto pod = connect_pod_a();
        flush_slint_events();
        CHECK(problems().empty());

        pod.reset();
        flush_slint_events();
        auto rows = problems();
        REQUIRE(rows.size() == 1);
        CHECK(std::string{rows.front().pod_id.data()} == "Pod-A");
        CHECK_THAT(std::string{rows.front().text.data()}, Catch::Matchers::StartsWith("Pod-A getrennt um "));
        CHECK(rows.front().gone);

        auto again = connect_pod_a();
        flush_slint_events();
        CHECK(problems().empty());
        REQUIRE(bridge->pod_model()->row_count() == 1);
    }

    SECTION("A pod on another protocol release is a problem")
    {
        auto pod = bridge->create_pod_state();
        pod->update_info({.id = cm::PodId{"Pod-A"}, .num_pumps = 1, .num_valves = 0});
        pod->update_state(cm::PodState::ConnectionState::incompatible);
        flush_slint_events();
        const auto rows = problems();
        REQUIRE(rows.size() == 1);
        CHECK_THAT(std::string{rows.front().text.data()}, Catch::Matchers::StartsWith("Pod-A hat eine inkompatible Firmware"));
    }

    SECTION("A pod without an id is no problem")
    {
        auto pod = bridge->create_pod_state();
        pod->update_state(cm::PodState::ConnectionState::incompatible);
        pod.reset();
        flush_slint_events();
        CHECK(problems().empty());
    }

    SECTION("Foreseen actions start disabled")
    {
        CHECK_FALSE(context.get_clean_available());
        CHECK_FALSE(context.get_reverse_available());
        CHECK_FALSE(context.get_restart_available());
        CHECK_FALSE(context.get_reconnect_available());
    }
}

TEST_CASE("StationStateBridge - Emergency stop reaches every pod", "[gui][StationStateBridge]")
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    auto ui = cm::gui::AppWindow::create();
    cm::PodRegistry pod_registry;
    cm::testing::TempDir dir;
    cm::FillLevelTracker fill_levels{ctx.get_executor(), dir.file("fill_levels.json")};
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui, pod_registry, scope, fill_levels);
    auto pod = std::make_shared<RefusingPod>();
    pod_registry.register_pod(pod);
    SlintFlusher flusher;

    ui->global<cm::gui::StationStateContext>().invoke_emergency_stop_all();
    REQUIRE(cm::run_until_complete(ctx, scope.join()));

    CHECK(pod->stops == 1);
}

TEST_CASE("StationStateBridge - Pods number their tiles one after the other", "[gui][StationStateBridge]")
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    auto ui = cm::gui::AppWindow::create();
    cm::PodRegistry pod_registry;
    cm::testing::TempDir dir;
    cm::FillLevelTracker fill_levels{ctx.get_executor(), dir.file("fill_levels.json")};
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui, pod_registry, scope, fill_levels);
    auto model = bridge->pod_model();
    SlintFlusher flusher;

    const auto connect = [&bridge](std::string id, int pumps) {
        auto pod = bridge->create_pod_state();
        pod->update_info({.id = cm::PodId{std::move(id)}, .num_pumps = pumps, .num_valves = 1});
        pod->update_state(cm::PodState::ConnectionState::connected);
        return pod;
    };
    auto first = connect("Pod-A", 3);
    auto second = connect("Pod-B", 2);
    auto third = connect("Pod-C", 1);
    flush_slint_events();

    REQUIRE(model->row_count() == 3);
    CHECK(model->row_data(0)->tile_offset == 0);
    CHECK(model->row_data(1)->tile_offset == 4);
    CHECK(model->row_data(2)->tile_offset == 7);

    first.reset();
    flush_slint_events();
    REQUIRE(model->row_count() == 2);
    CHECK(model->row_data(0)->tile_offset == 0);
    CHECK(model->row_data(1)->tile_offset == 3);
}
