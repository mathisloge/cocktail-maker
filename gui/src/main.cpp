#include <spdlog/spdlog.h>
import std;
import cm.core;
import cm.gui;
import cm.sim;

int main(int argc, char** argv)
{
    cm::gui::GuiApplication app;
    cm::log::set_level(cm::log::Level::debug);

    auto logger = cm::log::create_or_get("main");
    SPDLOG_LOGGER_INFO(logger, "Setup application...");
    try {
        app.init(DEBUG_LOCAL_DB_DIR);
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_CRITICAL(logger, "Could not initialize application. Error: {}", ex.what());
    }

    SPDLOG_LOGGER_INFO(logger, "Run application...");
    app.run(std::make_unique<cm::sim::SimulatedPodDiscovery>());
    SPDLOG_LOGGER_INFO(logger, "Application quit.");
    return 0;
}
