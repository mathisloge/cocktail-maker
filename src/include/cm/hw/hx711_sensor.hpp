#pragma once
#include <filesystem>
#include <gpiod.hpp>
#include <mp-units/framework.h>

namespace cm
{

struct InputPinDatConfig
{
    std::filesystem::path chip;
    gpiod::line::offset offset;
};
struct OutputPinClkConfig
{
    std::filesystem::path chip;
    gpiod::line::offset offset;
};

class Hx711Sensor
{
  public:
    Hx711Sensor(InputPinDatConfig dat_pin, OutputPinClkConfig clk_pin);

    std::int32_t read();
  private:
    void pulse_clock();

  private:
    gpiod::line_request dat_line_;
    gpiod::line::offset dat_offset_;

    gpiod::line_request clk_line_;
    gpiod::line::offset clk_offset_;
};
} // namespace cm
