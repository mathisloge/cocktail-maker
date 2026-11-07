#include <catch2/catch_test_macros.hpp>
#include <slint-testing.h>
#include <slint.h>
#include "app-window.h"

import std;

#include "slint_events.test.hpp"

namespace {
struct DetailPage
{
    slint::ComponentHandle<cm::gui::AppWindow> ui = cm::gui::AppWindow::create();
    std::vector<int> boosts;

    DetailPage()
    {
        ui->window().set_size(slint::LogicalSize({1280, 720}));
        ui->show();
        ui->global<cm::gui::RecipeContext>().on_boost_active_recipe([this](int boost) { boosts.push_back(boost); });
        auto& station = ui->global<cm::gui::StationStateContext>();
        station.set_station_ready(true);
        ui->set_boot_screen_confirmed(true);
        open();
    }

    DetailPage(const DetailPage&) = delete;
    DetailPage& operator=(const DetailPage&) = delete;

    ~DetailPage()
    {
        ui->hide();
    }

    void open()
    {
        show(cm::gui::Page::CocktailDetailPage);
    }

    // Slint creates and destroys the elements of an `if` only when it renders, so a page change runs the event loop
    // until the slider has appeared on the detail page or disappeared on any other page.
    void show(cm::gui::Page page)
    {
        ui->global<cm::gui::StationStateContext>().set_active_screen(page);
        const bool want_slider = page == cm::gui::Page::CocktailDetailPage;
        for (int frame = 0; frame < 100; ++frame) {
            slint::Timer::single_shot(std::chrono::milliseconds{20}, [] { slint::quit_event_loop(); });
            slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
            if (has_slider() == want_slider) {
                return;
            }
        }
        FAIL("the page did not change within 2 s");
    }

    [[nodiscard]] bool has_slider() const
    {
        return !slint::testing::ElementHandle::find_by_accessible_label(ui, "Alkohol-Boost").empty();
    }

    // The knob centre maps to the value, so a point is computed from the travel of the knob, not the full width.
    [[nodiscard]] slint::LogicalPosition point_at(float ratio, float offset = 0.0F) const
    {
        constexpr float knob = 40.0F;
        const auto position = slider().absolute_position();
        const auto size = slider().size();
        return slint::LogicalPosition{
            {position.x + (knob / 2) + (ratio * (size.width - knob)) + offset, position.y + (size.height / 2)}};
    }

    void tap(slint::LogicalPosition point)
    {
        ui->window().dispatch_pointer_press_event(point, slint::PointerEventButton::Left);
        ui->window().dispatch_pointer_release_event(point, slint::PointerEventButton::Left);
        flush_slint_events();
    }

    [[nodiscard]] slint::testing::ElementHandle slider() const
    {
        const auto found = slint::testing::ElementHandle::find_by_accessible_label(ui, "Alkohol-Boost");
        REQUIRE(found.size() == 1);
        return found[0];
    }

    [[nodiscard]] std::string value() const
    {
        return std::string{slider().accessible_value().value_or("")};
    }
};
} // namespace

TEST_CASE("boost slider snaps to 5 percent steps", "[boost_slider]")
{
    DetailPage page;
    SlintFlusher flusher;
    const auto slider = page.slider();

    CHECK(slider.accessible_value_step() == 5.0F);
    CHECK(slider.accessible_value_minimum() == -80.0F);
    CHECK(slider.accessible_value_maximum() == 80.0F);
    CHECK(page.value() == "0");

    slider.set_accessible_value("12");
    flush_slint_events();
    CHECK(page.value() == "10");
    REQUIRE_FALSE(page.boosts.empty());
    CHECK(page.boosts.back() == 10);

    slider.invoke_accessible_increment_action();
    flush_slint_events();
    CHECK(page.value() == "15");

    slider.invoke_accessible_decrement_action();
    slider.invoke_accessible_decrement_action();
    flush_slint_events();
    CHECK(page.value() == "5");

    slider.set_accessible_value("200");
    flush_slint_events();
    CHECK(page.value() == "80");

    slider.set_accessible_value("-200");
    flush_slint_events();
    CHECK(page.value() == "-80");
}

TEST_CASE("boost slider jumps to a tapped position", "[boost_slider]")
{
    DetailPage page;
    SlintFlusher flusher;

    page.tap(page.point_at(0.75F));

    CHECK(page.value() == "40");
}

TEST_CASE("boost slider keeps its value when the knob is grabbed off-centre", "[boost_slider]")
{
    DetailPage page;
    SlintFlusher flusher;

    page.tap(page.point_at(0.5F, 15.0F));

    CHECK(page.value() == "0");
    CHECK(page.boosts.empty());
}

TEST_CASE("boost slider is a large enough touch target", "[boost_slider]")
{
    DetailPage page;
    SlintFlusher flusher;

    CHECK(page.slider().size().height >= 48.0F);
}

TEST_CASE("boost slider starts at zero when the detail page opens again", "[boost_slider]")
{
    DetailPage page;
    SlintFlusher flusher;
    page.slider().set_accessible_value("35");
    flush_slint_events();
    REQUIRE(page.value() == "35");

    page.show(cm::gui::Page::CocktailListPage);
    page.open();

    CHECK(page.value() == "0");
}
