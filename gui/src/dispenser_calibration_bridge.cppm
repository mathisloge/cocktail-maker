module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/cobalt.hpp>
#include "app-window.h"

export module cm.gui:dispenser_calibration_bridge;

import std;
import cm;

namespace asio = boost::asio;
namespace cobalt = boost::cobalt;

namespace cm::gui {
export class DispenserCalibrationBridge
{

  public:
    explicit DispenserCalibrationBridge(asio::any_io_executor executor,
                                        slint::ComponentHandle<AppWindow> ui,
                                        PodRegistry& pod_registry)
        : executor_{std::move(executor)}
        , ui_{std::move(ui)}
        , pod_registry_{pod_registry}
    {
        ui_->global<DispenserCalibrationContext>().on_calibrate_offset(
            [this](gui::Pod pod, gui::Dispenser dispenser) { dispatch_load_cell_reset(std::move(pod), std::move(dispenser)); });
    }

  private:
    void dispatch_load_cell_reset(const gui::Pod pod, const gui::Dispenser dispenser)
    {
        asio::post(executor_, [pod_id = pod.id, dispenser_id = dispenser.id, this]() {
            async_dispatch_load_cell_reset(PodId{std::string{pod_id.data()}}, DispenserId{dispenser_id});
        });
    }

    cobalt::detached async_dispatch_load_cell_reset(const PodId pod_id, const DispenserId dispenser_id)
    {
        update_load_cell_offset_status(CalibrationStepStatus::Running);
        auto dispenser = pod_registry_.dispenser_of_pod(pod_id, dispenser_id);
        if (not dispenser.has_value()) {
            update_load_cell_offset_status(CalibrationStepStatus::Error);
            update_ui_error("Es konnte kein Dispenser gefunden werden.");
            co_return;
        }
        try {
            co_await (*dispenser)->load_cell_reset_offset();
            update_load_cell_offset_status(CalibrationStepStatus::Success);
        }
        catch (const std::exception& err) {
            update_load_cell_offset_status(CalibrationStepStatus::Error);
            update_ui_error(std::format("Es ist ein Fehler aufgetreten: {}.", err.what()));
        }
    }

    void update_ui_error(std::string error_str)
    {
        slint::invoke_from_event_loop([ui = ui_, error = std::move(error_str)]() {
            ui->global<DispenserCalibrationContext>().invoke_update_calibration_error(slint::SharedString{error.c_str()});
        });
    }

    void update_load_cell_offset_status(CalibrationStepStatus status)
    {
        slint::invoke_from_event_loop(
            [ui = ui_, status]() { ui->global<DispenserCalibrationContext>().invoke_update_offset_status(status); });
    }

  private:
    asio::any_io_executor executor_;
    slint::ComponentHandle<AppWindow> ui_;
    PodRegistry& pod_registry_;
};
} // namespace cm::gui
