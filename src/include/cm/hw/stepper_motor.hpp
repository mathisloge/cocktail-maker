#pragma once
#include <boost/asio/awaitable.hpp>
#include "cm/units.hpp"
#include "motor.hpp"
namespace cm
{
class StepperMotor : public Motor
{
  public:
    virtual boost::asio::awaitable<void> step(units::Steps steps, units::StepsPerSecond velocity) = 0;
    virtual boost::asio::awaitable<void> enable() = 0;
    virtual boost::asio::awaitable<void> disable() = 0;
};
} // namespace cm
