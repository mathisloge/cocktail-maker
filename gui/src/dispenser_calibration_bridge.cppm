module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/cobalt.hpp>
#include <libassert/assert-macros.hpp>
#include "app-window.h"

export module cm.gui:dispenser_calibration_bridge;

import std;
import cm;
import cm.core;
import mp_units;
import libassert;

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

        ui_->global<DispenserCalibrationContext>().on_calibrate_reference(
            [this](gui::Pod pod, gui::Dispenser dispenser, int grams) {
                dispatch_load_cell_set_ref_weight(std::move(pod), std::move(dispenser), (grams * units::si::gram));
            });

        ui_->global<DispenserCalibrationContext>().on_calibrate_pump([this](gui::Pod pod, gui::Dispenser dispenser, int steps) {
            dispatch_calibrate_pump(std::move(pod), std::move(dispenser), (steps * units::step));
        });
    }

  private:
    void dispatch_load_cell_reset(const gui::Pod pod, const gui::Dispenser dispenser)
    {
        asio::post(executor_, [pod_id = pod.id, dispenser_id = dispenser.id, this]() {
            async_dispatch_load_cell_reset(PodId{std::string{pod_id.data()}}, DispenserId{dispenser_id});
        });
    }

    void dispatch_load_cell_set_ref_weight(const gui::Pod pod, const gui::Dispenser dispenser, const units::Grams grams)
    {
        asio::post(executor_, [pod_id = pod.id, dispenser_id = dispenser.id, grams, this]() {
            async_dispatch_load_cell_set_ref_weight(PodId{std::string{pod_id.data()}}, DispenserId{dispenser_id}, grams);
        });
    }

    void dispatch_calibrate_pump(const gui::Pod pod, const gui::Dispenser dispenser, const units::Steps steps)
    {
        asio::post(executor_, [pod_id = pod.id, dispenser_id = dispenser.id, steps, this]() {
            async_calibrate_pump(PodId{std::string{pod_id.data()}}, DispenserId{dispenser_id}, steps);
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
            update_ui_error(err.what());
        }
    }

    cobalt::detached async_dispatch_load_cell_set_ref_weight(const PodId pod_id,
                                                             const DispenserId dispenser_id,
                                                             const units::Grams grams)
    {
        update_load_cell_ref_weight_status(CalibrationStepStatus::Running);
        auto dispenser = pod_registry_.dispenser_of_pod(pod_id, dispenser_id);
        if (not dispenser.has_value()) {
            update_load_cell_ref_weight_status(CalibrationStepStatus::Error);
            update_ui_error("Es konnte kein Dispenser gefunden werden.");
            co_return;
        }
        try {
            co_await (*dispenser)->load_cell_set_ref_weight(grams);
            update_load_cell_ref_weight_status(CalibrationStepStatus::Success);
        }
        catch (const std::exception& err) {
            update_load_cell_ref_weight_status(CalibrationStepStatus::Error);
            update_ui_error(err.what());
        }
    }

    cobalt::detached async_calibrate_pump(const PodId pod_id, const DispenserId dispenser_id, const units::Steps steps)
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

    void update_load_cell_ref_weight_status(CalibrationStepStatus status)
    {
        slint::invoke_from_event_loop(
            [ui = ui_, status]() { ui->global<DispenserCalibrationContext>().invoke_update_ref_weight_status(status); });
    }

    void update_pump_status(CalibrationStepStatus status)
    {
        slint::invoke_from_event_loop(
            [ui = ui_, status]() { ui->global<DispenserCalibrationContext>().invoke_update_pump_status(status); });
    }

    void update_pump_measured_calibration_value(units::Litre litre)
    {
        slint::invoke_from_event_loop([ui = ui_, litre]() {
            const auto str = std::format("{}", litre);
            ui->global<DispenserCalibrationContext>().invoke_update_pump_measured_calibrated_value(
                slint::SharedString{str.c_str()});
        });
    }

  private:
    asio::any_io_executor executor_;
    slint::ComponentHandle<AppWindow> ui_;
    PodRegistry& pod_registry_;
};
} // namespace cm::gui
