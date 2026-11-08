#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <slint-testing.h>
#include <slint.h>
#include "app-window.h"

import std;

#include "slint_events.test.hpp"

namespace {
using slint::testing::ElementHandle;

cm::gui::Dispenser pump(int id, std::string_view drink, float ratio, bool almost_empty = false)
{
    return cm::gui::Dispenser{
        .id = id,
        .type = cm::gui::DispenserType::Pump,
        .label = slint::SharedString{std::format("Pumpe {}", id + 1)},
        .ingredient_name = slint::SharedString{drink},
        .fill_text = "500 mL von 700 mL",
        .fill_known = true,
        .fill_ratio = ratio,
        .almost_empty = almost_empty,
    };
}

cm::gui::Pod pod_with(std::vector<cm::gui::Dispenser> dispensers)
{
    return cm::gui::Pod{
        .id = "Pod-A",
        .connection_state = cm::gui::ConnectionState::Connected,
        .firmware_version = "1.4.0",
        .pump_count = static_cast<int>(dispensers.size()),
        .dispensers = std::make_shared<slint::VectorModel<cm::gui::Dispenser>>(std::move(dispensers)),
    };
}

/// The admin area opened on a Station with one Pod, without the PIN that guards it in the app.
struct AdminAreaPage
{
    slint::ComponentHandle<cm::gui::AppWindow> ui = cm::gui::AppWindow::create();
    std::vector<int> refilled;
    std::vector<int> highlighted;
    int emergency_stops = 0;

    AdminAreaPage()
    {
        ui->window().set_size(slint::LogicalSize({1280, 720}));
        ui->show();
        auto& station = ui->global<cm::gui::StationStateContext>();
        station.on_mark_dispenser_refilled(
            [this](const cm::gui::Pod&, const cm::gui::Dispenser& dispenser) { refilled.push_back(dispenser.id); });
        station.on_highlight_dispenser(
            [this](const cm::gui::Pod&, const cm::gui::Dispenser& dispenser) { highlighted.push_back(dispenser.id); });
        station.on_emergency_stop_all([this]() { ++emergency_stops; });
        set_pods({pod_with({
            pump(0, "Weißer Rum", 0.64F),
            pump(1, "Limettensaft", 0.12F, true),
            pump(2, "", 0.0F),
        })});
        station.set_station_ready(true);
        ui->set_boot_screen_confirmed(true);
        station.set_active_screen(cm::gui::Page::AdminAreaPage);
        REQUIRE(wait_for("Pod-A Pumpe 1"));
    }

    AdminAreaPage(const AdminAreaPage&) = delete;
    AdminAreaPage& operator=(const AdminAreaPage&) = delete;

    ~AdminAreaPage()
    {
        ui->hide();
    }

    void set_pods(std::vector<cm::gui::Pod> pods)
    {
        ui->global<cm::gui::StationStateContext>().set_pods(std::make_shared<slint::VectorModel<cm::gui::Pod>>(std::move(pods)));
    }

    // Slint creates the elements of an `if` or `for` only when it renders, so this renders until one appears.
    [[nodiscard]] bool wait_for(std::string_view label) const
    {
        for (int frame = 0; frame < 100; ++frame) {
            render();
            if (not find(label).empty()) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool wait_until_gone(std::string_view label) const
    {
        for (int frame = 0; frame < 100; ++frame) {
            render();
            if (find(label).empty()) {
                return true;
            }
        }
        return false;
    }

    static void render()
    {
        slint::Timer::single_shot(std::chrono::milliseconds{20}, [] { slint::quit_event_loop(); });
        slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
    }

    [[nodiscard]] slint::SharedVector<ElementHandle> find(std::string_view label) const
    {
        return ElementHandle::find_by_accessible_label(ui, label);
    }

    [[nodiscard]] ElementHandle only(std::string_view label) const
    {
        const auto found = find(label);
        REQUIRE(found.size() == 1);
        return found[0];
    }

    void activate(std::string_view label) const
    {
        only(label).invoke_accessible_default_action();
        render();
    }

    /// The element with the label that lies inside the area of another element, like a button on a tile.
    [[nodiscard]] ElementHandle inside(std::string_view outer_label, std::string_view label) const
    {
        const auto outer = only(outer_label);
        const auto origin = outer.absolute_position();
        const auto size = outer.size();
        for (const auto& element : find(label)) {
            const auto position = element.absolute_position();
            if (position.x >= origin.x and position.y >= origin.y and position.x < origin.x + size.width and
                position.y < origin.y + size.height) {
                return element;
            }
        }
        FAIL(std::format("no '{}' inside '{}'", label, outer_label));
        return outer;
    }
};
} // namespace

TEST_CASE_METHOD(AdminAreaPage, "The admin area shows a tile per dispenser", "[admin_area]")
{
    CHECK_FALSE(find("Pod-A Pumpe 1").empty());
    CHECK_FALSE(find("Pod-A Pumpe 2").empty());
    CHECK_FALSE(find("Pod-A Pumpe 3").empty());
    CHECK_FALSE(find("Weißer Rum").empty());
}

TEST_CASE_METHOD(AdminAreaPage, "The admin area groups the tiles by pod", "[admin_area]")
{
    CHECK(find("Pod-A").size() == 1);
    // The Pod stands above its tiles, so a tile names only its dispenser.
    CHECK(find("Pod-A · Pumpe 1").empty());
    CHECK_FALSE(find("Pumpe 1").empty());
}

TEST_CASE_METHOD(AdminAreaPage, "The admin area assigns a drink from an unassigned tile", "[admin_area]")
{
    CHECK_FALSE(find("nicht belegt").empty());
    CHECK(find("Belegen").empty());

    activate("Pod-A Pumpe 3");

    CHECK(wait_for("Getränk ändern"));
}

TEST_CASE_METHOD(AdminAreaPage, "The admin area refills from the tile", "[admin_area]")
{
    inside("Pod-A Pumpe 2", "Nachgefüllt").invoke_accessible_default_action();
    render();

    CHECK(refilled == std::vector<int>{1});
}

TEST_CASE_METHOD(AdminAreaPage, "The admin area opens the pump page from a tile", "[admin_area]")
{
    activate("Pod-A Pumpe 1");

    CHECK(wait_for("← Pumpen"));
}

TEST_CASE_METHOD(AdminAreaPage, "The admin area scrolls with more than 12 tiles", "[admin_area]")
{
    std::vector<cm::gui::Dispenser> pumps;
    for (int id = 0; id < 16; ++id) {
        pumps.push_back(pump(id, "Weißer Rum", 0.5F));
    }
    set_pods({pod_with(std::move(pumps))});
    REQUIRE(wait_for("Pod-A Pumpe 12"));
    const auto tile_height = only("Pod-A Pumpe 1").size().height;
    const auto before = only("Pod-A Pumpe 9").absolute_position();

    ui->window().dispatch_pointer_scroll_event(slint::LogicalPosition{{before.x + 10.0F, before.y + 10.0F}}, 0.0F, -400.0F);
    render();

    CHECK(only("Pod-A Pumpe 9").absolute_position().y < before.y);
    REQUIRE(wait_for("Pod-A Pumpe 16"));
    const auto last = only("Pod-A Pumpe 16");
    CHECK(last.absolute_position().y + last.size().height <= 720.0F);
    // The tiles keep their size instead of being squeezed into the window.
    CHECK(last.size().height == tile_height);
    CHECK(tile_height >= 120.0F);
}

TEST_CASE_METHOD(AdminAreaPage, "The admin area shows a pod problem", "[admin_area]")
{
    ui->global<cm::gui::StationStateContext>().set_pod_problems(
        std::make_shared<slint::VectorModel<cm::gui::PodProblem>>(std::vector{
            cm::gui::PodProblem{.pod_id = "Pod-B", .text = "Pod-B getrennt um 21:12"},
        }));

    CHECK(wait_for("Pod-B getrennt um 21:12"));
    CHECK_FALSE(find("Details →").empty());
}

TEST_CASE_METHOD(AdminAreaPage, "The pump page shows foreseen actions as not available yet", "[admin_area]")
{
    activate("Pod-A Pumpe 1");
    REQUIRE(wait_for("Reinigen"));

    CHECK(only("Reinigen").accessible_enabled() == false);
    CHECK(only("Rückwärts leeren").accessible_enabled() == false);
    CHECK_FALSE(find("noch nicht verfügbar").empty());
}

TEST_CASE_METHOD(AdminAreaPage, "The pump page of a valve has no backwards run", "[admin_area]")
{
    auto valve = pump(3, "Soda", 0.5F);
    valve.type = cm::gui::DispenserType::Valve;
    valve.label = "Ventil 1";
    set_pods({pod_with({pump(0, "Weißer Rum", 0.64F), valve})});
    REQUIRE(wait_for("Pod-A Ventil 1"));

    activate("Pod-A Ventil 1");
    REQUIRE(wait_for("Reinigen"));

    CHECK(find("Rückwärts leeren").empty());
}

TEST_CASE_METHOD(AdminAreaPage, "The pump page identifies its dispenser", "[admin_area]")
{
    activate("Pod-A Pumpe 2");
    REQUIRE(wait_for("Identify"));

    activate("Identify");

    CHECK(highlighted == std::vector<int>{1});
}

TEST_CASE_METHOD(AdminAreaPage, "The pump page refills its dispenser", "[admin_area]")
{
    activate("Pod-A Pumpe 2");
    REQUIRE(wait_for("Identify"));

    activate("Nachgefüllt");

    CHECK(refilled == std::vector<int>{1});
}

TEST_CASE_METHOD(AdminAreaPage, "The pump page shows changes of its dispenser while open", "[admin_area]")
{
    activate("Pod-A Pumpe 1");
    REQUIRE(wait_for("Weißer Rum"));

    set_pods({pod_with({pump(0, "Dunkler Rum", 0.5F), pump(1, "Limettensaft", 0.12F, true)})});

    CHECK(wait_for("Dunkler Rum"));
    CHECK_FALSE(find("← Pumpen").empty());
}

TEST_CASE_METHOD(AdminAreaPage, "The pump page cannot be left while a calibration runs", "[admin_area]")
{
    activate("Pod-A Pumpe 1");
    REQUIRE(wait_for("Kalibrieren"));
    activate("Kalibrieren");
    REQUIRE(wait_for("Fertig"));

    auto& calibration = ui->global<cm::gui::DispenserCalibrationContext>();
    auto state = calibration.get_state();
    state.dispenser_status = cm::gui::CalibrationStepStatus::Running;
    calibration.set_state(state);
    render();

    CHECK(only("← Pumpen").accessible_enabled() == false);
    CHECK(only("← Zurück").accessible_enabled() == false);
    CHECK(only("Pumpen").accessible_enabled() == false);
    CHECK(only("Fertig").accessible_enabled() == false);

    state.dispenser_status = cm::gui::CalibrationStepStatus::Success;
    calibration.set_state(state);
    render();
    activate("Fertig");
    CHECK(wait_for("Identify"));
}

TEST_CASE_METHOD(AdminAreaPage, "The pump page goes back to the tiles", "[admin_area]")
{
    activate("Pod-A Pumpe 1");
    REQUIRE(wait_for("← Pumpen"));
    REQUIRE(find("Pod-A Pumpe 2").empty());

    activate("← Pumpen");

    CHECK(wait_for("Pod-A Pumpe 2"));
}

TEST_CASE_METHOD(AdminAreaPage, "The system section lists pods with their firmware", "[admin_area]")
{
    activate("System");

    CHECK(wait_for("Firmware 1.4.0"));
    CHECK_FALSE(find("3 Pumpen · 0 Ventile").empty());
    CHECK_FALSE(find("keine Fehler").empty());
}

TEST_CASE_METHOD(AdminAreaPage, "The system section stops every pod", "[admin_area]")
{
    activate("System");
    REQUIRE(wait_for("Not-Stopp alle Pods")); // codespell:ignore

    activate("Not-Stopp alle Pods"); // codespell:ignore

    CHECK(emergency_stops == 1);
}

TEST_CASE_METHOD(AdminAreaPage, "The system section offers restart and reconnect as not available yet", "[admin_area]")
{
    activate("System");
    REQUIRE(wait_for("Neustarten"));

    CHECK(only("Neustarten").accessible_enabled() == false);
    CHECK(only("Neu verbinden").accessible_enabled() == false);
}

TEST_CASE_METHOD(AdminAreaPage, "The system section lists problems of pods that are gone", "[admin_area]")
{
    ui->global<cm::gui::StationStateContext>().set_pod_problems(
        std::make_shared<slint::VectorModel<cm::gui::PodProblem>>(std::vector{
            cm::gui::PodProblem{.pod_id = "Pod-B", .text = "Pod-B getrennt um 21:12", .gone = true},
        }));
    REQUIRE(wait_for("Details →"));

    activate("Details →");

    CHECK(wait_for("Not-Stopp alle Pods")); // codespell:ignore
    CHECK_FALSE(find("Pod-B getrennt um 21:12").empty());
}

TEST_CASE_METHOD(AdminAreaPage, "The system section filters the log for errors", "[admin_area]")
{
    ui->global<cm::gui::StationStateContext>().set_log_entries(
        std::make_shared<slint::VectorModel<cm::gui::LogEntry>>(std::vector{
            cm::gui::LogEntry{
                .time_stamp = "21:00:00.000", .level = cm::gui::LogLevel::Info, .component = "ui", .message = "Mojito fertig"},
            cm::gui::LogEntry{
                .time_stamp = "21:00:01.000", .level = cm::gui::LogLevel::Err, .component = "pod", .message = "Timeout bei Ping"},
        }));
    activate("System");
    REQUIRE(wait_for("Mojito fertig"));

    activate("Fehler");

    CHECK(wait_for("Timeout bei Ping"));
    CHECK(find("Mojito fertig").empty());
}

TEST_CASE_METHOD(AdminAreaPage, "The pump page closes when its pod goes away", "[admin_area]")
{
    activate("Pod-A Pumpe 1");
    REQUIRE(wait_for("Identify"));

    set_pods({});

    CHECK(wait_until_gone("Identify"));
    CHECK(find("← Pumpen").empty());
}

TEST_CASE_METHOD(AdminAreaPage, "The window keeps the kiosk size on every admin section", "[admin_area]")
{
    activate("System");
    REQUIRE(wait_for("Not-Stopp alle Pods")); // codespell:ignore

    const auto size = ui->window().size();
    const float scale = ui->window().scale_factor();
    CHECK(static_cast<float>(size.width) / scale == Catch::Approx(1280.0F));
    CHECK(static_cast<float>(size.height) / scale == Catch::Approx(720.0F));
}

TEST_CASE_METHOD(AdminAreaPage, "The pump page splits its columns evenly", "[admin_area]")
{
    activate("Pod-A Pumpe 1");
    REQUIRE(wait_for("Pflege"));

    // Both columns share the width between the page padding, so the right one starts in the middle.
    constexpr float kPadding = 16.0F;
    constexpr float kGap = 16.0F;
    const float column = (1280.0F - (2 * kPadding) - kGap) / 2;
    CHECK(only("Pflege").absolute_position().x == Catch::Approx(kPadding + column + kGap).margin(1.0));
}
