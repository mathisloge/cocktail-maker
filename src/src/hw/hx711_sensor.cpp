#include "cm/hw/hx711_sensor.hpp"
#include <gpiod.hpp>

namespace cm
{

Hx711Sensor::Hx711Sensor(InputPinDatConfig dat_pin, OutputPinClkConfig clk_pin)
    : dat_line_{gpiod::chip{std::move(dat_pin.chip)}
                    .prepare_request()
                    .set_consumer("hx711")
                    .add_line_settings(dat_pin.offset,
                                       gpiod::line_settings{}.set_direction(gpiod::line::direction::INPUT))
                    .do_request()}
    , dat_offset_{dat_pin.offset}
    , clk_line_{gpiod::chip{std::move(clk_pin.chip)}
                    .prepare_request()
                    .set_consumer("hx711")
                    .add_line_settings(clk_pin.offset,
                                       gpiod::line_settings{}.set_direction(gpiod::line::direction::OUTPUT))
                    .do_request()}
    , clk_offset_{clk_pin.offset}
{}


} // namespace cm
