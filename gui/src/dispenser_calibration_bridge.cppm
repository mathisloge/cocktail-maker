module;
#include "app-window.h"

export module cm.gui:dispenser_calibration_bridge;

import std;
import cm.core;
import cm;

namespace cm::gui {
export class DispenserCalibrationBridge
{

  public:
    explicit DispenserCalibrationBridge(AsyncScope& async_scope,
                                        slint::ComponentHandle<AppWindow> ui,
                                        const PodRegistry& pod_registry);

    void init();

  private:
    void dispatch_load_tare(gui::Pod pod, gui::Dispenser dispenser);

    void dispatch_calibrate_load_cell_ref_weight(gui::Pod pod, gui::Dispenser dispenser, units::Grams grams);

    void dispatch_calibrate_dispenser(gui::Pod pod, gui::Dispenser dispenser, std::chrono::milliseconds run_time);

    Task<void> async_dispatch_load_tare(PodId pod_id, DispenserId dispenser_id);

    Task<void> async_dispatch_calibrate_load_cell_ref_weight(PodId pod_id, DispenserId dispenser_id, units::Grams grams);

    Task<void> async_calibrate_dispenser(PodId pod_id, DispenserId dispenser_id, std::chrono::milliseconds run_time);

    void update_ui_error(std::string error_str);

    void update_load_cell_calibration_status(CalibrationStepStatus status);

    void update_load_tare_status(CalibrationStepStatus status);

    void update_dispenser_status(CalibrationStepStatus status);

    void update_dispenser_measured_amount(units::Litre amount);

  private:
    AsyncScope& async_scope_;
    slint::ComponentHandle<AppWindow> ui_;
    const PodRegistry& pod_registry_;
};
} // namespace cm::gui
