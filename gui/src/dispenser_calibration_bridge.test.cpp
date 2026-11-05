#include <catch2/catch_test_macros.hpp>
#include <proto/field/ErrorCodeCommon.h>
#include <slint.h>
#include "app-window.h"

import std;
import cm.core;
import cm;
import cm.gui;

using cm::gui::CalibrationStep;
using ErrorCode = proto::field::ErrorCodeVal;

namespace {
template <typename Error>
std::string text_for(CalibrationStep step, Error error)
{
    return cm::gui::calibration_error_text(step, std::make_exception_ptr(std::move(error)));
}

cm::PodReceiveError nak(ErrorCode code)
{
    return cm::PodReceiveError{cm::PodId{"Pod"}, code};
}
} // namespace

// codespell:ignore-begin
TEST_CASE("Calibration errors become text an operator can act on", "[gui][calibration]")
{
    CHECK(text_for(CalibrationStep::dispenser_run, cm::DispenserEmptyError{cm::PodId{"Pod"}, cm::DispenserId{0}}) ==
          "Es ist fast nichts geflossen. Bitte Flasche, Schlauch und Motorstrom prüfen oder eine längere Laufzeit wählen. Die "
          "bisherige Kalibrierung bleibt gültig.");

    CHECK(text_for(CalibrationStep::clear_scale_tare, nak(ErrorCode::Busy)) ==
          "Auf diesem Dispenser läuft noch eine andere Aktion. Bitte warten und erneut versuchen.");

    CHECK(text_for(CalibrationStep::reference_weight, nak(ErrorCode::UnsupportedInCurrentState)) ==
          "Der Pod kann das gerade nicht ausführen. Bitte erneut versuchen.");

    CHECK(text_for(CalibrationStep::clear_scale_tare, nak(ErrorCode::LoadNotSteady)) ==
          "Die Last war nicht ruhig. Bitte die Waage nicht berühren und erneut versuchen.");

    CHECK(text_for(CalibrationStep::reference_weight, nak(ErrorCode::TareTooOld)) ==
          "Die Tara der leeren Waage fehlt oder ist älter als 5 Minuten. Bitte zuerst neu tarieren.");

    CHECK(text_for(CalibrationStep::reference_weight, nak(ErrorCode::ImplausibleReference)) ==
          "Das Referenzgewicht lag vermutlich nicht auf der Waage. Bitte auflegen und erneut versuchen.");

    CHECK(text_for(CalibrationStep::reference_weight, nak(ErrorCode::InvalidParameter)) ==
          "Das Referenzgewicht muss größer als 0 g sein.");

    CHECK(text_for(CalibrationStep::dispenser_run, nak(ErrorCode::NoLoadCell)) ==
          "Dieser Dispenser hat keine Wägezelle und lässt sich nicht kalibrieren.");

    CHECK(text_for(CalibrationStep::dispenser_run, nak(ErrorCode::InvalidParameter)) ==
          "Die Laufzeit ist ungültig. Erlaubt sind 1 bis 30 Sekunden.");

    CHECK(text_for(CalibrationStep::empty_bottle_tare, nak(ErrorCode::HardwareFault)) ==
          "Kein Motorstrom oder ein Sensor- oder Treiberfehler.");

    CHECK(text_for(CalibrationStep::dispenser_run, nak(ErrorCode::HardwareFault)) ==
          "Kein Motorstrom, ein Sensor- oder Treiberfehler oder der Lauf wurde gestoppt. Der Pod nutzt weiter die bisherige "
          "Kalibrierung. Bitte den Lauf wiederholen.");

    CHECK(text_for(CalibrationStep::dispenser_run, cm::PodTimeoutError{cm::PodId{"Pod"}}) ==
          "Der Pod hat nicht geantwortet. Bitte die USB-Verbindung prüfen.");

    CHECK(text_for(CalibrationStep::dispenser_run, std::runtime_error{"Etwas anderes"}) == "Etwas anderes");
}

// codespell:ignore-end
