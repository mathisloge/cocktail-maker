#include <catch2/catch_test_macros.hpp>
#include <slint.h>
#include "app-window.h"

import std;

#include "slint_events.test.hpp"

// Writes a picture of every screen, so that a UI change can be compared before and after.
// Run it with CM_SNAPSHOT_DIR=<dir> ./cocktail-maker-gui-tests "[.snapshot]".
// The FemtoVG renderer returns no snapshot, the software renderer does and renders the same pixels on every machine.
// It has to be selected before the first window, so run the test on its own.

namespace {
void write_ppm(const std::filesystem::path& file, const slint::SharedPixelBuffer<slint::Rgba8Pixel>& buffer)
{
    std::ofstream out{file, std::ios::binary};
    out << "P6\n" << buffer.width() << ' ' << buffer.height() << "\n255\n";
    for (const auto& pixel : std::span{buffer.begin(), buffer.end()}) {
        out.put(static_cast<char>(pixel.r));
        out.put(static_cast<char>(pixel.g));
        out.put(static_cast<char>(pixel.b));
    }
}

void snapshot(const slint::ComponentHandle<cm::gui::AppWindow>& ui, const std::filesystem::path& dir, std::string_view name)
{
    // Pages fade in after mounting, so the loop has to run until their animations are over.
    slint::Timer::single_shot(std::chrono::milliseconds{2500}, [] { slint::quit_event_loop(); });
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
    const auto buffer = ui->window().take_snapshot();
    REQUIRE(buffer.has_value());
    write_ppm(dir / std::format("{}.ppm", name), *buffer);
}

constexpr std::array<std::pair<cm::gui::Page, std::string_view>, 7> kPages{{
    {cm::gui::Page::CocktailListPage, "cocktail_list_page"},
    {cm::gui::Page::CocktailDetailPage, "cocktail_detail_page"},
    {cm::gui::Page::MixPage, "mix_page"},
    {cm::gui::Page::MixGlassSelectionPage, "mix_glass_selection_page"},
    {cm::gui::Page::MixErrorPage, "mix_error_page"},
    {cm::gui::Page::MixSuccessPage, "mix_success_page"},
    {cm::gui::Page::AdminAreaPage, "admin_area_page"},
}};
} // namespace

TEST_CASE("ui snapshots", "[.snapshot]")
{
    const char* const env_dir = std::getenv("CM_SNAPSHOT_DIR");
    if (env_dir == nullptr) {
        SUCCEED("CM_SNAPSHOT_DIR is not set");
        return;
    }
    const std::filesystem::path dir{env_dir};
    ::setenv("SLINT_BACKEND", "winit-software", 0);
    std::filesystem::create_directories(dir);

    auto ui = cm::gui::AppWindow::create();
    ui->window().set_size(slint::PhysicalSize({1280, 720}));
    ui->show();
    auto& station = ui->global<cm::gui::StationStateContext>();

    station.set_station_ready(false);
    snapshot(ui, dir, "boot_screen");

    station.set_station_ready(true);
    ui->set_boot_screen_confirmed(true);

    for (const auto& [page, name] : kPages) {
        station.set_active_screen(page);
        snapshot(ui, dir, name);
    }

    station.set_active_screen(cm::gui::Page::CocktailListPage);
    ui->invoke_open_fill_warning_popup();
    snapshot(ui, dir, "fill_warning_popup");
    ui->invoke_close_fill_warning_popup();

    ui->invoke_open_manual_command_popup(cm::gui::Command{});
    snapshot(ui, dir, "manual_command_popup");
    ui->invoke_close_manual_command_popup();

    ui->invoke_open_operator_pin_popup();
    snapshot(ui, dir, "operator_pin_popup");
    ui->invoke_close_operator_pin_popup();

    ui->hide();
    SlintFlusher flusher;
}

// Opens one screen and keeps the window open until it is closed, so that it can be inspected through Slint's MCP
// server (CM_SLINT_MCP). Screens such as the admin area are otherwise only reachable through a long press.
// Run it with CM_INSPECT_PAGE=admin_area_page SLINT_MCP_PORT=<port> ./cocktail-maker-gui-tests "[.inspect]".
TEST_CASE("ui inspect", "[.inspect]")
{
    const char* const env_page = std::getenv("CM_INSPECT_PAGE");
    if (env_page == nullptr) {
        SUCCEED("CM_INSPECT_PAGE is not set");
        return;
    }
    const auto entry = std::ranges::find(kPages, std::string_view{env_page}, &std::pair<cm::gui::Page, std::string_view>::second);
    REQUIRE(entry != kPages.end());

    auto ui = cm::gui::AppWindow::create();
    ui->window().set_size(slint::PhysicalSize({1280, 720}));
    auto& station = ui->global<cm::gui::StationStateContext>();
    station.set_station_ready(true);
    ui->set_boot_screen_confirmed(true);
    station.set_active_screen(entry->first);
    ui->run();
}
