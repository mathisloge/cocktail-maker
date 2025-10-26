// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <boost/asio/awaitable.hpp>
#include <gpiod.hpp>
#include <mp-units/systems/si.h>
#include "cm/units.hpp"
#include "pin_selection.hpp"
#include "weight_sensor.hpp"

namespace cm {

using Hx711DatPin = PinSelection<struct Hx711Dat>;
using Hx711ClkPin = PinSelection<struct Hx711Clk>;

// NOLINTBEGIN(readability-identifier-naming)
inline static constexpr struct hx711_unit final : units::named_unit<"hx711_raw", units::kind_of<units::isq::mass>>
{
} hx711_unit;

using Hx711RawValue = units::quantity<hx711_unit, std::int32_t>;

// NOLINTEND(readability-identifier-naming)

class Hx711Sensor : public WeightSensor
{
  public:
    Hx711Sensor(Hx711DatPin dat_pin, Hx711ClkPin clk_pin);
    OperationState state() const override;
    boost::asio::awaitable<void> tare() override;
    boost::asio::awaitable<void> calibrate_with_ref_weight(units::Grams known_mass) override;

  private:
    [[nodiscard]] boost::asio::awaitable<units::quantity_point<units::si::gram>> read() override;
    [[nodiscard]] boost::asio::awaitable<units::quantity_point<hx711_unit>> read_raw();
    void pulse_clock();

  private:
    OperationState state_{OperationState::initializing};

    gpiod::line_request dat_line_;
    gpiod::line::offset dat_offset_;

    gpiod::line_request clk_line_;
    gpiod::line::offset clk_offset_;

    units::quantity_point<hx711_unit> offset_{};
    units::quantity_point<hx711_unit> raw_value_{};
    units::Grams known_mass_{};
};
} // namespace cm
