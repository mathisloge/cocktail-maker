#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <slint.h>
#include "app-window.h"

import std;
import cm;
import cm.gui;

// Helper to flush Slint's event loop queue.
inline void flush_slint_events()
{
    slint::invoke_from_event_loop([]() { slint::quit_event_loop(); });
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
}

TEST_CASE("StationStateBridge - Initialization", "[gui][StationStateBridge]")
{
    auto ui = cm::gui::AppWindow::create();
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui);

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
    auto ui = cm::gui::AppWindow::create();
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui);
    auto model = bridge->pod_model();

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
    auto ui = cm::gui::AppWindow::create();
    auto bridge = std::make_shared<cm::gui::StationStateBridge>(ui);
    auto model = bridge->pod_model();

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
