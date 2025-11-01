// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <boost/asio/awaitable.hpp>
#include "cm/async.hpp"
#include "cm/units.hpp"
#include "motor.hpp"

namespace cm {
class StepperMotor : public Motor
{
  public:
    virtual async<void> step(units::Steps steps, units::StepsPerSecond velocity) = 0;
    virtual async<void> enable() = 0;
    virtual async<void> disable() = 0;
};
} // namespace cm
