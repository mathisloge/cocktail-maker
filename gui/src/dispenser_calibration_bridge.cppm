module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/cobalt/task.hpp>
#include "app-window.h"

export module cm.gui:dispenser_calibration_bridge;

import std;
import cm.core;
import cm;

namespace asio = boost::asio;
namespace cobalt = boost::cobalt;

namespace cm::gui {
export class DispenserCalibrationBridge
{

  public:
    explicit DispenserCalibrationBridge(asio::any_io_executor executor,
                                        slint::ComponentHandle<AppWindow> ui,
                                        const PodRegistry& pod_registry);

    void init();

  private:
    void dispatch_load_tare(gui::Pod pod, gui::Dispenser dispenser);

    void dispatch_calibrate_load_cell_ref_weight(gui::Pod pod, gui::Dispenser dispenser, units::Grams grams);

    void dispatch_calibrate_pump(gui::Pod pod, gui::Dispenser dispenser, units::Steps steps);

    cobalt::task<void> async_dispatch_load_tare(PodId pod_id, DispenserId dispenser_id);

    cobalt::task<void> async_dispatch_calibrate_load_cell_ref_weight(PodId pod_id, DispenserId dispenser_id, units::Grams grams);

    cobalt::task<void> async_calibrate_pump(PodId pod_id, DispenserId dispenser_id, units::Steps steps);

    void update_ui_error(std::string error_str);

    void update_load_cell_calibration_status(CalibrationStepStatus status);

    void update_load_tare_status(CalibrationStepStatus status);

    void update_pump_status(CalibrationStepStatus status);

    void update_pump_measured_calibration_value(units::Litre litre);

  private:
    asio::any_io_executor executor_;
    slint::ComponentHandle<AppWindow> ui_;
    const PodRegistry& pod_registry_;
};
} // namespace cm::gui
