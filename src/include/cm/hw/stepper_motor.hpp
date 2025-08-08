// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <boost/asio/awaitable.hpp>
#include "cm/units.hpp"
#include "motor.hpp"

namespace cm {
class StepperMotor : public Motor
{
  public:
    virtual boost::asio::awaitable<void> step(units::Steps steps, units::StepsPerSecond velocity) = 0;
    virtual boost::asio::awaitable<void> enable() = 0;
    virtual boost::asio::awaitable<void> disable() = 0;
};
} // namespace cm
