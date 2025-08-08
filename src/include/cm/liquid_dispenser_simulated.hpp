// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "liquid_dispenser.hpp"

namespace cm {
class SimulatedLiquidDispenser : public LiquidDispenser
{
  public:
    SimulatedLiquidDispenser(
        units::Litre capacity,
        mp_units::quantity<mp_units::si::litre / mp_units::si::second> flow_rate);
    boost::asio::awaitable<void> dispense(units::Litre volume) override;
    units::Litre remaining_volume() const override;
    void refill(units::Litre volume) override;
    const std::string& name() const override;

  private:
    units::Litre remaining_capacity_;
    mp_units::quantity<mp_units::si::litre / mp_units::si::second> flow_rate_;
};
} // namespace cm
