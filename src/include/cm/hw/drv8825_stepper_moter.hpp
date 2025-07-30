#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include "pin_selection.hpp"
#include "stepper_motor.hpp"
#include <mp-units/systems/si.h>

namespace cm
{
using Drv8825EnablePin = PinSelection<struct Drv8825EnablePinSel>;
using Drv8825StepPin = PinSelection<struct Drv8825StepPinSel>;
using Drv8825DirectionPin = PinSelection<struct Drv8825DirectionPinSel>;

using StepsPerSecond = decltype((1 * step) / (1 * mp_units::si::second));
using StepsPerSecondSq = decltype(StepsPerSecond{} / mp_units::si::second);

class Drv8825StepperMotorDriver : public StepperMotor
{
  public:
    Drv8825StepperMotorDriver(Drv8825EnablePin enable_pin, Drv8825StepPin step_pin, Drv8825DirectionPin direction_pin);
    boost::asio::awaitable<void> step(Steps steps, StepsPerSecond velocity);

  private:
    boost::asio::awaitable<void> step_one(boost::asio::steady_timer &timer, std::chrono::microseconds wait_after);

  private:
    gpiod::line_request enable_line_;
    gpiod::line::offset enable_offset_;

    gpiod::line_request step_line_;
    gpiod::line::offset step_offset_;

    gpiod::line_request direction_line_;
    gpiod::line::offset direction_offset_;

    static constexpr StepsPerSecondSq kAcceleration = 2000 * (cm::step / (mp_units::si::second * mp_units::si::second));
};
} // namespace cm
