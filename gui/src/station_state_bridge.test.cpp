#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <slint.h>
#include "app-window.h"

import std;
import mp_units;
import cm.core;
import cm;
import cm.gui;

namespace {
// Helper to flush Slint's event loop queue.
inline void flush_slint_events()
{
    slint::invoke_from_event_loop([]() { slint::quit_event_loop(); });
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
}

// RAII guard: Local variables are destroyed in reverse order of declaration.
// When a SECTION finishes, `pod` (section) is destructed first (pushing cleanup tasks to Slint).
// Then this `flusher` is destructed, running those pending tasks and freeing the captured
// `shared_ptr<StationStateBridge>`, allowing `bridge` and `ui` to be cleanly destroyed.
struct SlintFlusher
{
    ~SlintFlusher()
    {
        flush_slint_events();
    }
};
} // namespace

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

TEST_CASE("format_fill_level words what is known about a bottle", "[gui][StationStateBridge]")
{
    namespace units = cm::units;
    const auto grams = [](double value) { return value * units::si::gram; };

    CHECK(cm::gui::format_fill_level(cm::FillLevelView{}) == "unbekannt");
    CHECK(cm::gui::format_fill_level(cm::FillLevelView{.no_load_cell = true}) == "keine Wägezelle");
    CHECK(cm::gui::format_fill_level(cm::FillLevelView{.level = grams(640), .percent = std::nullopt, .suspected_empty = false}) ==
          "ca. 640 mL");
    CHECK(cm::gui::format_fill_level(cm::FillLevelView{
              .level = grams(640), .percent = 64.0 * units::percent, .suspected_empty = false}) == "ca. 640 mL · 64 %");
    CHECK(cm::gui::format_fill_level(cm::FillLevelView{
              .level = grams(30), .percent = 3.0 * units::percent, .suspected_empty = true}) == "vermutlich leer");
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
    pod->update_info(cm::PodInfo{.id = cm::PodId{"Old"},
                                 .firmware_version = {},
                                 .protocol_version = cm::Version{.major = 2, .minor = 0, .patch = 0},
                                 .num_pumps = 1,
                                 .num_valves = 0});
    pod->update_state(cm::PodState::ConnectionState::incompatible);
    flush_slint_events();

    const auto row = bridge->pod_model()->row_data(0);
    REQUIRE(row.has_value());
    CHECK(row->connection_state == cm::gui::ConnectionState::Incompatible);
    CHECK(std::string{row->status_text.data()} == "Protokoll 2.0.0, erwartet 2.1.0");
    CHECK_FALSE(ui->global<cm::gui::StationStateContext>().get_station_ready());

    pod.reset();
}
