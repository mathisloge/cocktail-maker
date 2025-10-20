// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/hw/hx711_sensor.hpp"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <gpiod.hpp>
#include <mp-units/systems/si.h>
#include <mp-units/systems/usc.h>
#include <thread>

namespace cm {
constexpr auto kClockDelay = std::chrono::microseconds(1); // ~1us

Hx711Sensor::Hx711Sensor(boost::asio::io_context& io, Hx711DatPin dat_pin, Hx711ClkPin clk_pin)
    : WeightSensor{"hx711", io}
    , dat_line_{gpiod::chip{std::move(dat_pin.chip)}
                    .prepare_request()
                    .set_consumer("hx711-dat")
                    .add_line_settings(dat_pin.offset, gpiod::line_settings{}.set_direction(gpiod::line::direction::INPUT))
                    .do_request()}
    , dat_offset_{dat_pin.offset}
    , clk_line_{gpiod::chip{std::move(clk_pin.chip)}
                    .prepare_request()
                    .set_consumer("hx711-clk")
                    .add_line_settings(clk_pin.offset, gpiod::line_settings{}.set_direction(gpiod::line::direction::OUTPUT))
                    .do_request()}
    , clk_offset_{clk_pin.offset}
{
    boost::asio::post([this]() {
        pulse_clock();
        state_ = OperationState::ok;
    });
}

OperationState Hx711Sensor::state() const
{
    return state_;
}

void Hx711Sensor::pulse_clock()
{
    clk_line_.set_value(clk_offset_, gpiod::line::value::ACTIVE);
    clk_line_.set_value(clk_offset_, gpiod::line::value::INACTIVE);
}

boost::asio::awaitable<void> Hx711Sensor::tare()
{
    constexpr int kSteps = 10;
    state_ = OperationState::calibrating;
    boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};
    units::quantity<hx711_unit> measure_points{};
    for (int i = 0; i < kSteps; i++) {
        co_await timer.async_wait(boost::asio::use_awaitable);
        timer.expires_after(std::chrono::milliseconds{100});
        measure_points += (co_await read_raw()).quantity_from_zero();
    }
    offset_ = units::quantity_point<hx711_unit>{measure_points / kSteps};
    state_ = OperationState::ok;
}

boost::asio::awaitable<void> Hx711Sensor::calibrate_with_ref_weight(units::Grams known_mass)
{
    constexpr int kSteps = 10;
    state_ = OperationState::calibrating;
    boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};
    units::quantity<hx711_unit> measure_points{};
    for (int i = 0; i < kSteps; i++) {
        co_await timer.async_wait(boost::asio::use_awaitable);
        timer.expires_after(std::chrono::milliseconds{100});
        measure_points += (co_await read_raw()).quantity_from_zero();
    }
    raw_value_ = units::quantity_point<hx711_unit>{measure_points / kSteps};
    known_mass_ = known_mass;
    state_ = OperationState::ok;
}

boost::asio::awaitable<units::quantity_point<hx711_unit>> Hx711Sensor::read_raw()
{
    // Wait for data line to go LOW (data ready)
    while (dat_line_.get_value(dat_offset_) != gpiod::line::value::INACTIVE) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    std::uint32_t value = 0;

    for (int i = 0; i < 24; ++i) {
        pulse_clock();
        value <<= 1;
        auto bit = static_cast<std::uint8_t>(dat_line_.get_value(dat_offset_));
        value |= (bit & 0x01);
    }

    // 25th pulse to set gain (128 for channel A)
    pulse_clock();

    if ((value & 0x800000) != 0U) {
        value |= 0xFF000000; // fill upper 8 bits
    }
    co_return static_cast<std::int32_t>(value) * hx711_unit;
}

boost::asio::awaitable<units::quantity_point<units::si::gram>> Hx711Sensor::read()
{
    const auto delta = raw_value_ - offset_;
    const auto scale = known_mass_ / delta;
    const auto raw = co_await read_raw();
    co_return (raw - offset_) * scale;
}
} // namespace cm
