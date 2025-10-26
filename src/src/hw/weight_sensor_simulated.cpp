// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/hw/weight_sensor_simulated.hpp"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace cm {
WeightSensorSimulated::WeightSensorSimulated()
    : WeightSensor{"SimulatedWeightSensor"}
{
    state_ = OperationState::ok; // NOLINT
}

OperationState WeightSensorSimulated::state() const
{
    return state_;
}

boost::asio::awaitable<void> WeightSensorSimulated::tare()
{
    state_ = OperationState::calibrating;
    co_await boost::asio::steady_timer{co_await boost::asio::this_coro::executor, std::chrono::seconds{1}} //
        .async_wait(boost::asio::use_awaitable);
    state_ = OperationState::ok;
    co_return;
}

boost::asio::awaitable<void> WeightSensorSimulated::calibrate_with_ref_weight(units::Grams known_mass)
{
    state_ = OperationState::calibrating;
    co_await boost::asio::steady_timer{co_await boost::asio::this_coro::executor, std::chrono::seconds{1}} //
        .async_wait(boost::asio::use_awaitable);
    known_mass_ = known_mass;
    state_ = OperationState::ok;
    co_return;
}

boost::asio::awaitable<units::quantity_point<units::si::gram>> WeightSensorSimulated::read()
{
    if (state_ == OperationState::calibrating) {
        co_return 0 * units::si::gram;
    }
    co_await boost::asio::steady_timer{co_await boost::asio::this_coro::executor, std::chrono::milliseconds{10}} //
        .async_wait(boost::asio::use_awaitable);
    co_return 100 * units::si::gram;
}
} // namespace cm
