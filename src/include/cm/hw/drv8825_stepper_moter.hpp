// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <mp-units/systems/si.h>
#include "cm/units.hpp"
#include "pin_selection.hpp"
#include "stepper_motor.hpp"

namespace cm {
using Drv8825EnablePin = PinSelection<struct Drv8825EnablePinSel>;
using Drv8825StepPin = PinSelection<struct Drv8825StepPinSel>;
using Drv8825DirectionPin = PinSelection<struct Drv8825DirectionPinSel>;

class Drv8825StepperMotorDriver : public StepperMotor
{
  public:
    Drv8825StepperMotorDriver(Drv8825EnablePin enable_pin,
                              Drv8825StepPin step_pin,
                              Drv8825DirectionPin direction_pin);
    boost::asio::awaitable<void> step(units::Steps steps, units::StepsPerSecond velocity) override;
    boost::asio::awaitable<void> enable() override;
    boost::asio::awaitable<void> disable() override;

  private:
    boost::asio::awaitable<void> step_one(boost::asio::steady_timer& timer,
                                          std::chrono::microseconds wait_after);

  private:
    gpiod::line_request enable_line_;
    gpiod::line::offset enable_offset_;

    gpiod::line_request step_line_;
    gpiod::line::offset step_offset_;

    gpiod::line_request direction_line_;
    gpiod::line::offset direction_offset_;

    static constexpr units::StepsPerSecondSq kAcceleration =
        500 * (units::step / units::pow<2>(units::si::second));
};
} // namespace cm
