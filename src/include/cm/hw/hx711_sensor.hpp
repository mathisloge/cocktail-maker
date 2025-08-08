// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <filesystem>
#include <boost/asio/awaitable.hpp>
#include <gpiod.hpp>
#include "cm/units.hpp"
#include "pin_selection.hpp"
#include <mp-units/systems/si.h>

namespace cm
{

using Hx711DatPin = PinSelection<struct Hx711Dat>;
using Hx711ClkPin = PinSelection<struct Hx711Clk>;

inline static constexpr struct Hx711Unit final : mp_units::named_unit<"hx711_raw", mp_units::one>
{
} hx711_unit;
struct Hx711RawValue final : mp_units::quantity<hx711_unit, std::int32_t>
{};

class Hx711Sensor
{
  public:
    Hx711Sensor(Hx711DatPin dat_pin, Hx711ClkPin clk_pin);
    boost::asio::awaitable<void> tare();
    boost::asio::awaitable<void> calibrate_with_ref_weight(units::Grams known_mass);
    [[nodiscard]] boost::asio::awaitable<units::Grams> read();

  private:
    [[nodiscard]] boost::asio::awaitable<Hx711RawValue> read_raw();
    void pulse_clock();

  private:
    gpiod::line_request dat_line_;
    gpiod::line::offset dat_offset_;

    gpiod::line_request clk_line_;
    gpiod::line::offset clk_offset_;

    Hx711RawValue offset_{};
    Hx711RawValue raw_value_{};
    units::Grams known_mass_{};
};
} // namespace cm
