#include <thread>
#include <cm/hw/hx711_sensor.hpp>
#include <fmt/core.h>

using namespace mp_units::si::unit_symbols;

int main()
{

    cm::Hx711Sensor load_cell{cm::InputPinDatConfig{.chip = "/dev/gpiochip0", .offset = {24}},
                              cm::OutputPinClkConfig{.chip = "/dev/gpiochip0", .offset = {23}}};
    fmt::println("start");
    std::this_thread::sleep_for(std::chrono::milliseconds{1000});
    load_cell.tare();

    fmt::println("place ref weight");
    std::this_thread::sleep_for(std::chrono::milliseconds{5000});
    load_cell.calibrate(load_cell.read_raw(), 100 * g);

    while (true)
    {
        fmt::println("read: {}", load_cell.read_raw().in<std::int32_t>());
        fmt::println("read: {}", load_cell.read());
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }
    return 0;
}
