#pragma once
#include <filesystem>
#include <gpiod.hpp>
#include <mp-units/systems/si.h>

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

inline static constexpr struct Hx711Unit final : mp_units::named_unit<"hx711_raw", mp_units::one>
{
} hx711_unit;
struct Hx711RawValue final : mp_units::quantity<hx711_unit, std::int32_t>
{};

class Hx711Sensor
{
  public:
    Hx711Sensor(InputPinDatConfig dat_pin, OutputPinClkConfig clk_pin);
    void tare();

    [[nodiscard]] Hx711RawValue read_raw();
    [[nodiscard]] mp_units::quantity<mp_units::si::gram> read();

  private:
    void pulse_clock();

  private:
    gpiod::line_request dat_line_;
    gpiod::line::offset dat_offset_;

    gpiod::line_request clk_line_;
    gpiod::line::offset clk_offset_;

    Hx711RawValue offset_{};
    mp_units::quantity<mp_units::si::gram> known_mass_{};
    Hx711RawValue raw_value_{};
};
} // namespace cm
