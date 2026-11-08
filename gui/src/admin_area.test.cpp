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
        .dispensers = std::make_shared<slint::VectorModel<cm::gui::Dispenser>>(std::move(dispensers)),
    };
}

/// The admin area opened on a Station with one Pod, without the PIN that guards it in the app.
struct AdminAreaPage
{
    slint::ComponentHandle<cm::gui::AppWindow> ui = cm::gui::AppWindow::create();
    std::vector<int> refilled;

    AdminAreaPage()
    {
        ui->window().set_size(slint::LogicalSize({1280, 720}));
        ui->show();
        auto& station = ui->global<cm::gui::StationStateContext>();
        station.on_mark_dispenser_refilled(
            [this](const cm::gui::Pod&, const cm::gui::Dispenser& dispenser) { refilled.push_back(dispenser.id); });
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
    CHECK_FALSE(find("nicht belegt").empty());
    CHECK_FALSE(find("Belegen").empty());
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
