// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/machine_state.hpp"
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include "cm/hw/hx711_sensor.hpp"

namespace cm {

MachineState::MachineState(boost::asio::io_context& io, std::unique_ptr<Hx711Sensor> load_cell)
    : load_cell_{std::move(load_cell)}
{
    boost::asio::co_spawn(
        io,
        [this] -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};

            fmt::println("start");
            timer.expires_after(std::chrono::seconds{1});
            co_await timer.async_wait(boost::asio::use_awaitable);
            co_await load_cell_->tare();

            fmt::println("place ref weight");
            timer.expires_after(std::chrono::seconds{5});
            co_await timer.async_wait(boost::asio::use_awaitable);
            co_await load_cell_->calibrate_with_ref_weight(100 * mp_units::si::gram);

            while (true) {
                measured_weight_ = co_await load_cell_->read();
                fmt::println("read: {}", measured_weight_);
                timer.expires_after(std::chrono::milliseconds{100});
                co_await timer.async_wait(boost::asio::use_awaitable);
            }
            co_return;
        },
        boost::asio::bind_cancellation_slot(cancel_signal_.slot(), boost::asio::detached));
}

MachineState::~MachineState()
{
    cancel_signal_.emit(boost::asio::cancellation_type::all);
}

units::OperationalState MachineState::load_cell_status() const
{
    return units::OperationalState::ok;
}

units::Grams MachineState::last_measured_weight() const
{
    return measured_weight_;
}

} // namespace cm
