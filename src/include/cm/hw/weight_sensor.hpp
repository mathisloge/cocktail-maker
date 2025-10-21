// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <boost/asio/awaitable.hpp>
#include <spdlog/fwd.h>
#include "cm/operation_state.hpp"
#include "cm/units.hpp"

namespace cm {
class WeightSensor
{
  public:
    struct MeasurePoint
    {
        units::quantity_point<units::si::gram> weight;
        std::chrono::system_clock::time_point time;
    };

    explicit WeightSensor(std::string name, const boost::asio::any_io_executor& io);
    virtual ~WeightSensor();
    WeightSensor(const WeightSensor&) = delete;
    WeightSensor(WeightSensor&&) noexcept = delete;
    WeightSensor& operator=(const WeightSensor&) = delete;
    WeightSensor& operator=(WeightSensor&&) = delete;

    [[nodiscard]] MeasurePoint last_measure() const;
    [[nodiscard]] virtual OperationState state() const = 0;
    [[nodiscard]] virtual boost::asio::awaitable<void> tare() = 0;
    [[nodiscard]] virtual boost::asio::awaitable<void> calibrate_with_ref_weight(units::Grams known_mass) = 0;

  protected:
    [[nodiscard]] virtual boost::asio::awaitable<units::quantity_point<units::si::gram>> read() = 0;

  private:
    std::shared_ptr<spdlog::logger> logger_;
    boost::asio::cancellation_signal cancel_signal_;
    MeasurePoint measure_;
};
} // namespace cm
