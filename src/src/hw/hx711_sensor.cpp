// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/hw/hx711_sensor.hpp"
#include <thread>
#include <gpiod.hpp>

#include <mp-units/systems/si.h>
#include <mp-units/systems/usc.h>

namespace cm
{
constexpr auto kClockDelay = std::chrono::microseconds(1); // ~1us
Hx711Sensor::Hx711Sensor(Hx711DatPin dat_pin, Hx711ClkPin clk_pin)
    : dat_line_{gpiod::chip{std::move(dat_pin.chip)}
                    .prepare_request()
                    .set_consumer("hx711-dat")
                    .add_line_settings(dat_pin.offset,
                                       gpiod::line_settings{}.set_direction(gpiod::line::direction::INPUT))
                    .do_request()}
    , dat_offset_{dat_pin.offset}
    , clk_line_{gpiod::chip{std::move(clk_pin.chip)}
                    .prepare_request()
                    .set_consumer("hx711-clk")
                    .add_line_settings(clk_pin.offset,
                                       gpiod::line_settings{}.set_direction(gpiod::line::direction::OUTPUT))
                    .do_request()}
    , clk_offset_{clk_pin.offset}
{
    boost::asio::post([this]() { pulse_clock(); });
}

void Hx711Sensor::pulse_clock()
{
    clk_line_.set_value(clk_offset_, gpiod::line::value::ACTIVE);
    clk_line_.set_value(clk_offset_, gpiod::line::value::INACTIVE);
}

boost::asio::awaitable<void> Hx711Sensor::tare()
{
    offset_ = co_await read_raw();
}

boost::asio::awaitable<void> Hx711Sensor::calibrate_with_ref_weight(mp_units::quantity<mp_units::si::gram> known_mass)
{
    raw_value_ = co_await read_raw();
    known_mass_ = known_mass;
}

boost::asio::awaitable<Hx711RawValue> Hx711Sensor::read_raw()
{
    // Wait for data line to go LOW (data ready)
    while (dat_line_.get_value(dat_offset_) != gpiod::line::value::INACTIVE)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    std::uint32_t value = 0;

    for (int i = 0; i < 24; ++i)
    {
        pulse_clock();
        value <<= 1;
        auto bit = static_cast<std::uint8_t>(dat_line_.get_value(dat_offset_));
        value |= (bit & 0x01);
    }

    // 25th pulse to set gain (128 for channel A)
    pulse_clock();

    if ((value & 0x800000) != 0U)
    {
        value |= 0xFF000000; // fill upper 8 bits
    }
    co_return Hx711RawValue{static_cast<std::int32_t>(value) * hx711_unit};
}

boost::asio::awaitable<units::Grams> Hx711Sensor::read()
{
    const auto delta = raw_value_ - offset_;
    const auto scale = known_mass_ / delta;
    const auto raw = co_await read_raw();
    co_return (raw - offset_) * scale;
}
} // namespace cm
