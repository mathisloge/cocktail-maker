// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/liquid_dispenser_simulated.hpp"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace cm
{
SimulatedLiquidDispenser::SimulatedLiquidDispenser(
    units::Litre capacity, mp_units::quantity<mp_units::si::litre / mp_units::si::second> flow_rate)
    : remaining_capacity_{capacity}
    , flow_rate_{flow_rate}
{}

boost::asio::awaitable<void> SimulatedLiquidDispenser::dispense(units::Litre volume)
{
    boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};

    remaining_capacity_ -= volume;
    timer.expires_after(
        std::chrono::duration_cast<std::chrono::microseconds>(mp_units::to_chrono_duration(volume / flow_rate_)));
    co_await timer.async_wait(boost::asio::use_awaitable);
}

units::Litre SimulatedLiquidDispenser::remaining_volume() const
{
    return remaining_capacity_;
}

void SimulatedLiquidDispenser::refill(units::Litre volume)
{
    remaining_capacity_ = volume;
}

const std::string &SimulatedLiquidDispenser::name() const
{
    static const std::string kName = "SimulatedLiquidDispenser";
    return kName;
}

} // namespace cm
