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
    app.run(std::make_unique<cm::sim::SimulatedPodDiscovery>());
#else
    app.run(std::make_unique<cm::SerialPodDiscovery>());
#endif
    SPDLOG_LOGGER_INFO(logger, "Application quit.");
    return 0;
}
