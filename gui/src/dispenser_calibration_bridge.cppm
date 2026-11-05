module;
#include "app-window.h"

export module cm.gui:dispenser_calibration_bridge;

import std;
import mp_units;
import cm.core;
import cm;

namespace cm::gui {

/// A run below this leaves the calibration rough, because the load cell only reads to about 1 g.
export inline constexpr units::Litre kRecommendedCalibrationAmount = 50.0 * units::milli_litre;

export enum class CalibrationStep
{
    clear_scale_tare,
    reference_weight,
    empty_bottle_tare,
    dispenser_run,
};

/// Words a failed step for the operator.
export [[nodiscard]] std::string calibration_error_text(CalibrationStep step, const std::exception_ptr& error);

export class DispenserCalibrationBridge
{

  public:
    explicit DispenserCalibrationBridge(AsyncScope& async_scope,
                                        slint::ComponentHandle<AppWindow> ui,
                                        const PodRegistry& pod_registry,
                                        FillLevelTracker& fill_levels);

    void init();

  private:
    Task<void> async_dispatch_load_tare(PodId pod_id, DispenserId dispenser_id);

    Task<void> async_dispatch_calibrate_load_cell_ref_weight(PodId pod_id, DispenserId dispenser_id, units::Grams grams);

    Task<void> async_dispatch_empty_bottle_tare(PodId pod_id, DispenserId dispenser_id);

    Task<void> async_calibrate_dispenser(PodId pod_id, DispenserId dispenser_id, std::chrono::milliseconds run_time);

    void update_ui_error(std::string error_str);

    void update_load_cell_calibration_status(CalibrationStepStatus status);

    void update_load_tare_status(CalibrationStepStatus status);

    void update_empty_bottle_tare_status(CalibrationStepStatus status);

    void update_dispenser_status(CalibrationStepStatus status);

    void update_dispenser_measured_amount(units::Litre amount);

    void update_reference_reading(std::string reading);

  private:
    AsyncScope& async_scope_;
    slint::ComponentHandle<AppWindow> ui_;
    const PodRegistry& pod_registry_;
    FillLevelTracker& fill_levels_;
};
} // namespace cm::gui
