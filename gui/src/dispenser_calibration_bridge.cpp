module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/cobalt/spawn.hpp>
#include <boost/cobalt/task.hpp>
#include <libassert/assert-macros.hpp>
#include <mp-units/systems/si/units.h>
#include "app-window.h"

module cm.gui:dispenser_calibration_bridge_impl;

import std;
import cm;
import cm.core;
import libassert;
import :dispenser_calibration_bridge;

namespace asio = boost::asio;
namespace cobalt = boost::cobalt;

namespace cm::gui {
DispenserCalibrationBridge::DispenserCalibrationBridge(asio::any_io_executor executor,
                                                       slint::ComponentHandle<AppWindow> ui,
                                                       const PodRegistry& pod_registry)
    : executor_{std::move(executor)}
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

    ui_->global<DispenserCalibrationContext>().on_calibrate_pump([this](gui::Pod pod, gui::Dispenser dispenser, int steps) {
        dispatch_calibrate_pump(std::move(pod), std::move(dispenser), (steps * units::step));
    });
}

void DispenserCalibrationBridge::dispatch_load_tare(const gui::Pod pod, const gui::Dispenser dispenser)
{
    boost::cobalt::spawn(
        executor_, async_dispatch_load_tare(PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}), boost::asio::detached);
}

void DispenserCalibrationBridge::dispatch_calibrate_load_cell_ref_weight(const gui::Pod pod,
                                                                         const gui::Dispenser dispenser,
                                                                         const units::Grams grams)
{
    boost::cobalt::spawn(
        executor_,
        async_dispatch_calibrate_load_cell_ref_weight(PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}, grams),
        boost::asio::detached);
}

void DispenserCalibrationBridge::dispatch_calibrate_pump(const gui::Pod pod,
                                                         const gui::Dispenser dispenser,
                                                         const units::Steps steps)
{
    boost::cobalt::spawn(executor_,
                         async_calibrate_pump(PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}, steps),
                         boost::asio::detached);
}

cobalt::task<void> DispenserCalibrationBridge::async_dispatch_load_tare(const PodId pod_id, const DispenserId dispenser_id)
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

cobalt::task<void> DispenserCalibrationBridge::async_dispatch_calibrate_load_cell_ref_weight(const PodId pod_id,
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

cobalt::task<void> DispenserCalibrationBridge::async_calibrate_pump(const PodId pod_id,
                                                                    const DispenserId dispenser_id,
                                                                    const units::Steps steps)
{
    update_pump_status(CalibrationStepStatus::Running);
    auto dispenser = pod_registry_.dispenser_of_pod(pod_id, dispenser_id);
    if (not dispenser.has_value()) {
        update_pump_status(CalibrationStepStatus::Error);
        update_ui_error("Es konnte kein Dispenser gefunden werden.");
        co_return;
    }
    auto* pump = dynamic_cast<Pump*>(dispenser.value().get());
    ASSERT(pump != nullptr, "Dispenser needs to be a pump");

    try {
        const auto pumped = co_await pump->calibrate(steps);
        update_pump_measured_calibration_value(pumped);
        update_pump_status(CalibrationStepStatus::Success);
    }
    catch (const std::exception& err) {
        update_pump_status(CalibrationStepStatus::Error);
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

void DispenserCalibrationBridge::update_pump_status(CalibrationStepStatus status)
{
    slint::invoke_from_event_loop(
        [ui = ui_, status]() { ui->global<DispenserCalibrationContext>().invoke_update_pump_status(status); });
}

void DispenserCalibrationBridge::update_pump_measured_calibration_value(units::Litre litre)
{
    const auto str = std::format("{}", litre);
    slint::invoke_from_event_loop([ui = ui_, litre = slint::SharedString{str.c_str()}]() {
        ui->global<DispenserCalibrationContext>().invoke_update_pump_measured_calibrated_value(litre);
    });
}
} // namespace cm::gui
