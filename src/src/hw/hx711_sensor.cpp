#include "cm/hw/hx711_sensor.hpp"
#include <thread>
#include <gpiod.hpp>

#include <mp-units/systems/si.h>
#include <mp-units/systems/usc.h>

namespace cm
{
constexpr auto kClockDelay = std::chrono::microseconds(1); // ~1us
Hx711Sensor::Hx711Sensor(InputPinDatConfig dat_pin, OutputPinClkConfig clk_pin)
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
    clk_line_.set_value(clk_offset_, gpiod::line::value::ACTIVE);
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
    clk_line_.set_value(clk_offset_, gpiod::line::value::INACTIVE);
}

void Hx711Sensor::pulse_clock()
{
    clk_line_.set_value(clk_offset_, gpiod::line::value::ACTIVE);
    clk_line_.set_value(clk_offset_, gpiod::line::value::INACTIVE);
}

void Hx711Sensor::tare()
{
    offset_ = read_raw();
}

Hx711RawValue Hx711Sensor::read_raw()
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
    return Hx711RawValue{static_cast<std::int32_t>(value) * hx711_unit};
}

mp_units::quantity<mp_units::si::gram> Hx711Sensor::read()
{
    mp_units::quantity<mp_units::si::gram> known_mass{};
    mp_units::quantity<hx711_unit> raw_value{};

    auto delta = raw_value - offset_;
    auto scale_ = known_mass / delta;

    return (read_raw() - offset_) * scale_;
}
} // namespace cm
