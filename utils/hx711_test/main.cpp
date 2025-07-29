#include <thread>
#include <cm/hw/hx711_sensor.hpp>
#include <fmt/core.h>

int main()
{

    cm::Hx711Sensor load_cell{cm::InputPinDatConfig{.chip = "/dev/gpiochip0", .offset = {24}},
                              cm::OutputPinClkConfig{.chip = "/dev/gpiochip0", .offset = {23}}};
    fmt::println("start");
    while (true)
    {
        fmt::println("read: {}", load_cell.read());
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }
    return 0;
}
