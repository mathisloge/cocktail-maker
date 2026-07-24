#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include "config.hpp"
import std;
import cm.core;
import cm.gui;
import cm;
#if BUILD_SIMULATED == 1
import cm.sim;
#endif

int main(int argc, char** argv)
{
    CLI::App cli{"Sir-Mix-A-Lot cocktail-mixing station"};
    bool fullscreen = false;
    cli.add_flag("--fullscreen", fullscreen, "Run the kiosk UI in fullscreen mode");
    CLI11_PARSE(cli, argc, argv);

    cm::gui::GuiApplication app;
    cm::log::set_level(cm::log::Level::debug);

    auto logger = cm::log::create_or_get("main");
    SPDLOG_LOGGER_INFO(logger, "Setup application...");

    try {
        app.init(std::filesystem::current_path() / "db");
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_CRITICAL(logger, "Could not initialize application. Error: {}", ex.what());
    }

    SPDLOG_LOGGER_INFO(logger, "Run application...");
#if BUILD_SIMULATED == 1
    app.run(std::make_unique<cm::sim::SimulatedPodDiscovery>(), fullscreen);
#else
    app.run(std::make_unique<cm::SerialPodDiscovery>(), fullscreen);
#endif
    SPDLOG_LOGGER_INFO(logger, "Application quit.");
    return 0;
}
