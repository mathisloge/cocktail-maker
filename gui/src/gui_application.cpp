module;
#include <spdlog/spdlog.h>
#include "app-window.h"

module cm.gui:gui_application_impl;
import cm.core;
import cm;
import :gui_application;
import :ui_log_sink;
import :station_state_bridge;

namespace cm::gui {

GuiApplication::GuiApplication()
{
    auto ui_log_sink = std::make_shared<ui_log_sink_st>();
    log::add_sink(ui_log_sink);

    const auto& ui_station_state_context = ui_->global<StationStateContext>();
    ui_station_state_context.set_log_entries(ui_log_sink->model());
}

void GuiApplication::init(const std::filesystem::path& db_dir)
{
    Application::init(db_dir);
    dispenser_calibration_bridge_.init();
    recipe_context_bridge_.init();
    glass_context_bridge_.init();
    process_context_bridge_.init();
}

void GuiApplication::run(std::unique_ptr<PodDiscovery> pod_discovery, bool fullscreen)
{
    auto station_state = std::make_shared<StationStateBridge>(ui_, pod_registry_, async_scope());
    // don't move station_state, keep it alive as long as the ui is running
    Application::run(station_state, std::move(pod_discovery));
    ui_->window().set_fullscreen(fullscreen);
    ui_->run();
    // The asynchronous work references the bridges and station_state, so it has to finish before they are destroyed.
    shutdown();
}

} // namespace cm::gui
