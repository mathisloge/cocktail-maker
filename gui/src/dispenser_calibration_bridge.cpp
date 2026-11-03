module;
#include <stdexec/execution.hpp>
#include "app-window.h"

module cm.gui:dispenser_calibration_bridge_impl;

import std;
import cm;
import cm.core;
import mp_units;
import :dispenser_calibration_bridge;

namespace cm::gui {
DispenserCalibrationBridge::DispenserCalibrationBridge(AsyncScope& async_scope,
                                                       slint::ComponentHandle<AppWindow> ui,
                                                       const PodRegistry& pod_registry)
    : async_scope_{async_scope}
    , ui_{std::move(ui)}
    , pod_registry_{pod_registry}
{
}

void DispenserCalibrationBridge::init()
{
    ui_->global<DispenserCalibrationContext>().on_calibrate_load_cell_ref_weight(
        [this](gui::Pod pod, gui::Dispenser dispenser, int grams) {
            dispatch_calibrate_load_cell_ref_weight(std::move(pod), std::move(dispenser), (grams * units::si::gram));
        });
    ui_->global<DispenserCalibrationContext>().on_load_cell_tare(
        [this](gui::Pod pod, gui::Dispenser dispenser) { dispatch_load_tare(std::move(pod), std::move(dispenser)); });

    ui_->global<DispenserCalibrationContext>().on_calibrate_dispenser(
        [this](gui::Pod pod, gui::Dispenser dispenser, int run_time_s) {
            dispatch_calibrate_dispenser(std::move(pod), std::move(dispenser), std::chrono::seconds{run_time_s});
        });
}

void DispenserCalibrationBridge::dispatch_load_tare(const gui::Pod pod, const gui::Dispenser dispenser)
{
    async_scope_.spawn(async_dispatch_load_tare(PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}));
}

void DispenserCalibrationBridge::dispatch_calibrate_load_cell_ref_weight(const gui::Pod pod,
                                                                         const gui::Dispenser dispenser,
                                                                         const units::Grams grams)
{
    async_scope_.spawn(
        async_dispatch_calibrate_load_cell_ref_weight(PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}, grams));
}

void DispenserCalibrationBridge::dispatch_calibrate_dispenser(const gui::Pod pod,
                                                              const gui::Dispenser dispenser,
                                                              const std::chrono::milliseconds run_time)
{
    async_scope_.spawn(async_calibrate_dispenser(PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}, run_time));
}

Task<void> DispenserCalibrationBridge::async_dispatch_load_tare(const PodId pod_id, const DispenserId dispenser_id)
{
    update_load_tare_status(CalibrationStepStatus::Running);
    auto dispenser = pod_registry_.dispenser_of_pod(pod_id, dispenser_id);
    if (not dispenser.has_value()) {
        update_load_tare_status(CalibrationStepStatus::Error);
        update_ui_error("Es konnte kein Dispenser gefunden werden.");
        co_return;
    }
    try {
        co_await (*dispenser)->load_cell_tare();
        update_load_tare_status(CalibrationStepStatus::Success);
    }
    catch (const std::exception& err) {
        update_load_tare_status(CalibrationStepStatus::Error);
        update_ui_error(err.what());
    }
}

Task<void> DispenserCalibrationBridge::async_dispatch_calibrate_load_cell_ref_weight(const PodId pod_id,
                                                                                     const DispenserId dispenser_id,
                                                                                     const units::Grams grams)
{
    update_load_cell_calibration_status(CalibrationStepStatus::Running);
    auto dispenser = pod_registry_.dispenser_of_pod(pod_id, dispenser_id);
    if (not dispenser.has_value()) {
        update_load_cell_calibration_status(CalibrationStepStatus::Error);
        update_ui_error("Es konnte kein Dispenser gefunden werden.");
        co_return;
    }
    try {
        co_await (*dispenser)->load_cell_calibrate_with_ref_weight(grams);
        update_load_cell_calibration_status(CalibrationStepStatus::Success);
    }
    catch (const std::exception& err) {
        update_load_cell_calibration_status(CalibrationStepStatus::Error);
        update_ui_error(err.what());
    }
}

Task<void> DispenserCalibrationBridge::async_calibrate_dispenser(const PodId pod_id,
                                                                 const DispenserId dispenser_id,
                                                                 const std::chrono::milliseconds run_time)
{
    update_dispenser_status(CalibrationStepStatus::Running);
    auto dispenser = pod_registry_.dispenser_of_pod(pod_id, dispenser_id);
    if (not dispenser.has_value()) {
        update_dispenser_status(CalibrationStepStatus::Error);
        update_ui_error("Es konnte kein Dispenser gefunden werden.");
        co_return;
    }

    try {
        const units::Litre amount = co_await (*dispenser)->calibrate(run_time);
        update_dispenser_measured_amount(amount);
        update_dispenser_status(CalibrationStepStatus::Success);
    }
    catch (const std::exception& err) {
        update_dispenser_status(CalibrationStepStatus::Error);
        update_ui_error(err.what());
    }
}

void DispenserCalibrationBridge::update_ui_error(std::string error_str)
{
    slint::invoke_from_event_loop([ui = ui_, error = std::move(error_str)]() {
        ui->global<DispenserCalibrationContext>().invoke_update_calibration_error(slint::SharedString{error.c_str()});
    });
}

void DispenserCalibrationBridge::update_load_cell_calibration_status(CalibrationStepStatus status)
{
    slint::invoke_from_event_loop(
        [ui = ui_, status]() { ui->global<DispenserCalibrationContext>().invoke_update_load_cell_ref_weight_status(status); });
}

void DispenserCalibrationBridge::update_load_tare_status(CalibrationStepStatus status)
{
    slint::invoke_from_event_loop(
        [ui = ui_, status]() { ui->global<DispenserCalibrationContext>().invoke_update_load_cell_tare_status(status); });
}

void DispenserCalibrationBridge::update_dispenser_status(CalibrationStepStatus status)
{
    slint::invoke_from_event_loop(
        [ui = ui_, status]() { ui->global<DispenserCalibrationContext>().invoke_update_dispenser_status(status); });
}

void DispenserCalibrationBridge::update_dispenser_measured_amount(units::Litre amount)
{
    const auto text = std::format("{} mL", std::lround(amount.numerical_value_in(units::milli_litre)));
    slint::invoke_from_event_loop([ui = ui_, amount = slint::SharedString{text.c_str()}]() {
        ui->global<DispenserCalibrationContext>().invoke_update_dispenser_measured_amount(amount);
    });
}
} // namespace cm::gui
