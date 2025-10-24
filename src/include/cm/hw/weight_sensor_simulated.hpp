// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "weight_sensor.hpp"

namespace cm {
class WeightSensorSimulated : public WeightSensor
{
  public:
    explicit WeightSensorSimulated(const boost::asio::any_io_executor& io);
    [[nodiscard]] OperationState state() const override;
    [[nodiscard]] boost::asio::awaitable<void> tare() override;
    [[nodiscard]] boost::asio::awaitable<void> calibrate_with_ref_weight(units::Grams known_mass) override;

  private:
    boost::asio::awaitable<units::quantity_point<units::si::gram>> read() override;

  private:
    OperationState state_{OperationState::initializing};
    units::Grams known_mass_{};
};
} // namespace cm
