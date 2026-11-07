module;
#include <proto/field/ErrorCodeCommon.h>
#include <stdexec/execution.hpp>
#include "app-window.h"

module cm.gui;

import std;
import cm;
import cm.core;
import mp_units;

namespace cm::gui {

namespace {
// codespell:ignore-begin
constexpr auto kDispenserNotFoundText = "Es konnte kein Dispenser gefunden werden.";
constexpr auto kKeepsCalibrationText = " Der Pod nutzt weiter die bisherige Kalibrierung. Bitte den Lauf wiederholen.";

/// The field of the dialog's state that shows how `step` is doing.
CalibrationStepStatus DispenserCalibrationState::* status_of(const CalibrationStep step)
{
    switch (step) {
    case CalibrationStep::clear_scale_tare:
        return &DispenserCalibrationState::load_cell_tare_status;
    case CalibrationStep::reference_weight:
        return &DispenserCalibrationState::load_cell_status;
    case CalibrationStep::empty_bottle_tare:
        return &DispenserCalibrationState::empty_bottle_tare_status;
    case CalibrationStep::dispenser_run:
        return &DispenserCalibrationState::dispenser_status;
    }
    std::unreachable();
}

std::string receive_error_text(const CalibrationStep step, const PodReceiveError& error)
{
    using ErrorCode = proto::field::ErrorCodeCommon::ValueType;
    switch (error.error_code()) {
    case ErrorCode::Busy:
        return "Auf diesem Dispenser läuft noch eine andere Aktion. Bitte warten und erneut versuchen.";
    case ErrorCode::UnsupportedInCurrentState:
        return "Der Pod kann das gerade nicht ausführen. Bitte erneut versuchen.";
    case ErrorCode::LoadNotSteady:
        return "Die Last war nicht ruhig. Bitte die Waage nicht berühren und erneut versuchen.";
    case ErrorCode::TareTooOld:
        return "Die Tara der leeren Waage fehlt oder ist älter als 5 Minuten. Bitte zuerst neu tarieren.";
    case ErrorCode::ImplausibleReference:
        return "Das Referenzgewicht lag vermutlich nicht auf der Waage. Bitte auflegen und erneut versuchen.";
    case ErrorCode::NoLoadCell:
        return "Dieser Dispenser hat keine Wägezelle und lässt sich nicht kalibrieren.";
    case ErrorCode::InvalidParameter:
        if (step == CalibrationStep::reference_weight) {
            return "Das Referenzgewicht muss größer als 0 g sein.";
        }
        if (step == CalibrationStep::dispenser_run) {
            return "Die Laufzeit ist ungültig. Erlaubt sind 1 bis 30 Sekunden.";
        }
        break;
    case ErrorCode::HardwareFault:
        if (step == CalibrationStep::dispenser_run) {
            return std::string{"Kein Motorstrom, ein Sensor- oder Treiberfehler oder der Lauf wurde gestoppt."} +
                   kKeepsCalibrationText;
        }
        return "Kein Motorstrom oder ein Sensor- oder Treiberfehler.";
    case ErrorCode::DispenserNotFound:
    case ErrorCode::NotCalibrated:
    case ErrorCode::InternalError:
    case ErrorCode::DispenserEmpty:
    case ErrorCode::ValuesLimit:
        break;
    }
    return error.what();
}

// codespell:ignore-end
} // namespace

std::string calibration_error_text(const CalibrationStep step, const std::exception_ptr& error)
{
    // codespell:ignore-begin
    try {
        std::rethrow_exception(error);
    }
    catch (const DispenserEmptyError&) {
        return "Es ist fast nichts geflossen. Bitte Flasche, Schlauch und Motorstrom prüfen oder eine längere Laufzeit wählen. "
               "Die bisherige Kalibrierung bleibt gültig.";
    }
    catch (const PodReceiveError& receive_error) {
        return receive_error_text(step, receive_error);
    }
    catch (const PodTimeoutError&) {
        return "Der Pod hat nicht geantwortet. Bitte die USB-Verbindung prüfen.";
    }
    catch (const PodDisconnectedError&) {
        return "Der Pod ist nicht mehr verbunden. Bitte die USB-Verbindung prüfen.";
    }
    catch (const std::exception& other) {
        return other.what();
    }
    // codespell:ignore-end
}

DispenserCalibrationBridge::DispenserCalibrationBridge(AsyncScope& async_scope,
                                                       slint::ComponentHandle<AppWindow> ui,
                                                       const PodRegistry& pod_registry,
                                                       FillLevelTracker& fill_levels)
    : async_scope_{async_scope}
    , ui_{std::move(ui)}
    , pod_registry_{pod_registry}
    , fill_levels_{fill_levels}
{
}

void DispenserCalibrationBridge::init()
{
    const auto& context = ui_->global<DispenserCalibrationContext>();
    context.on_load_cell_tare([this](const gui::Pod pod, const gui::Dispenser dispenser) {
        async_scope_.spawn(async_dispatch_load_tare(PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}));
    });
    context.on_calibrate_load_cell_ref_weight([this](const gui::Pod pod, const gui::Dispenser dispenser, const int grams) {
        async_scope_.spawn(async_dispatch_calibrate_load_cell_ref_weight(
            PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}, grams * units::si::gram));
    });
    context.on_empty_bottle_tare([this](const gui::Pod pod, const gui::Dispenser dispenser) {
        async_scope_.spawn(async_dispatch_empty_bottle_tare(PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}));
    });
    context.on_calibrate_dispenser([this](const gui::Pod pod, const gui::Dispenser dispenser, const int run_time_s) {
        async_scope_.spawn(async_calibrate_dispenser(
            PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}, std::chrono::seconds{run_time_s}));
    });
    context.on_stop_calibration([this]() { async_scope_.spawn(pod_registry_.force_safe_state_all_pods()); });
}

template <typename Body>
Task<void> DispenserCalibrationBridge::run_step(const CalibrationStep step, const PodDispenser address, Body body)
{
    const auto status = status_of(step);
    update_state([status](DispenserCalibrationState& state) {
        state.*status = CalibrationStepStatus::Running;
        state.error_message = "";
    });

    auto dispenser = pod_registry_.dispenser_of_pod(address.pod_id, address.dispenser_id);
    if (not dispenser.has_value()) {
        update_state([status](DispenserCalibrationState& state) {
            state.*status = CalibrationStepStatus::Error;
            state.error_message = kDispenserNotFoundText;
        });
        co_return;
    }

    std::exception_ptr failure;
    try {
        co_await body(**dispenser);
    }
    catch (const std::exception&) {
        failure = std::current_exception();
    }

    if (failure == nullptr) {
        update_state([status](DispenserCalibrationState& state) { state.*status = CalibrationStepStatus::Success; });
        co_return;
    }
    const auto text = calibration_error_text(step, failure);
    update_state([status, text](DispenserCalibrationState& state) {
        state.*status = CalibrationStepStatus::Error;
        state.error_message = slint::SharedString{text};
    });
}

Task<void> DispenserCalibrationBridge::async_dispatch_load_tare(const PodId pod_id, const DispenserId dispenser_id)
{
    const PodDispenser address{pod_id, dispenser_id};
    co_await run_step(CalibrationStep::clear_scale_tare, address, [this, address](cm::Dispenser& dispenser) -> Task<void> {
        co_await dispenser.load_cell_tare();
        // The zero point has moved and the bottle is off the scale, so the level isn't known anymore.
        fill_levels_.clear_scale_tared(address);
    });
}

Task<void> DispenserCalibrationBridge::async_dispatch_calibrate_load_cell_ref_weight(const PodId pod_id,
                                                                                     const DispenserId dispenser_id,
                                                                                     const units::Grams grams)
{
    update_state([](DispenserCalibrationState& state) { state.reference_reading = ""; });
    co_await run_step(
        CalibrationStep::reference_weight,
        PodDispenser{pod_id, dispenser_id},
        [this, grams](cm::Dispenser& dispenser) -> Task<void> {
            co_await dispenser.load_cell_calibrate_with_ref_weight(grams);

            // The scale should now show the known weight, which lets the operator check the result. A failed check leaves
            // the calibration itself successful.
            try {
                const units::Grams reading = co_await dispenser.load_cell_weight();
                const auto text = std::format("{} g", std::lround(reading.numerical_value_in(units::si::gram)));
                update_state([text](DispenserCalibrationState& state) { state.reference_reading = slint::SharedString{text}; });
            }
            catch (const std::exception&) {
                const auto text = calibration_error_text(CalibrationStep::reference_weight, std::current_exception());
                update_state([text](DispenserCalibrationState& state) { state.error_message = slint::SharedString{text}; });
            }
        });
}

Task<void> DispenserCalibrationBridge::async_dispatch_empty_bottle_tare(const PodId pod_id, const DispenserId dispenser_id)
{
    const PodDispenser address{pod_id, dispenser_id};
    co_await run_step(CalibrationStep::empty_bottle_tare, address, [this, address](cm::Dispenser& dispenser) -> Task<void> {
        co_await dispenser.load_cell_tare();
        fill_levels_.empty_bottle_tared(address);
    });
}

Task<void> DispenserCalibrationBridge::async_calibrate_dispenser(const PodId pod_id,
                                                                 const DispenserId dispenser_id,
                                                                 const std::chrono::milliseconds run_time)
{
    const PodDispenser address{pod_id, dispenser_id};
    co_await run_step(CalibrationStep::dispenser_run, address, [this, address, run_time](cm::Dispenser& dispenser) -> Task<void> {
        // A catch handler can't await, so the failure waits here until the fill level is up to date.
        std::exception_ptr failure;
        std::optional<units::Litre> amount;
        try {
            amount = co_await dispenser.calibrate(run_time);
        }
        catch (const PodTimeoutError&) {
            // The Pod starts the dispenser before it replies, so it may be pouring with nobody watching.
            async_scope_.spawn(pod_registry_.force_safe_state_all_pods());
            failure = std::current_exception();
        }
        catch (const std::exception&) {
            failure = std::current_exception();
        }

        if (amount.has_value()) {
            const units::Litre rounded = std::lround(amount->numerical_value_in(units::milli_litre)) * units::milli_litre;
            const auto text = std::format("{} mL", std::lround(rounded.numerical_value_in(units::milli_litre)));
            const bool below_recommended = rounded < kRecommendedCalibrationAmount;
            update_state([text, below_recommended](DispenserCalibrationState& state) {
                state.measured_amount = slint::SharedString{text};
                state.below_recommended_amount = below_recommended;
            });
            co_await fill_levels_.pour_finished(
                address, dispenser, DispenseReport{.amount = *amount, .outcome = DispenseOutcome::completed});
        }
        else {
            // Liquid may have come out even though the run failed.
            co_await fill_levels_.refresh(address, dispenser);
        }

        if (failure != nullptr) {
            std::rethrow_exception(failure);
        }
    });
}

void DispenserCalibrationBridge::update_state(std::function<void(DispenserCalibrationState&)> change)
{
    slint::invoke_from_event_loop([ui = ui_, change = std::move(change)]() {
        const auto& context = ui->global<DispenserCalibrationContext>();
        auto state = context.get_state();
        change(state);
        context.set_state(state);
    });
}
} // namespace cm::gui
