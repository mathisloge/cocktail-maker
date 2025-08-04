#include <boost/asio.hpp>
#include <cm/hw/drv8825_stepper_moter.hpp>
#include <cm/liquid_dispenser_stepper_pump.hpp>
#include <cm/machine_state.hpp>
#include "cm/hw/hx711_sensor.hpp"
int main()
{
    boost::asio::io_context io;
    std::shared_ptr<cm::MachineState> machine_state = std::make_shared<cm::MachineState>(
        io,
        std::make_unique<cm::Hx711Sensor>(cm::Hx711DatPin{.chip = "/dev/gpiochip0", .offset = {23}},
                                          cm::Hx711ClkPin{.chip = "/dev/gpiochip0", .offset = {24}}));

    cm::StepperPumpLiquidDispenser liquid_dispenser{
        "test-dispenser",
        std::make_unique<cm::Drv8825StepperMotorDriver>(
            cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {17}},
            cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {27}},
            cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {22}}),
        (1000 * cm::units::step) / (100 * mp_units::si::milli<mp_units::si::litre>),
        52 * mp_units::si::milli<mp_units::si::litre>};

    boost::asio::co_spawn(
        io,
        [&] -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};

            fmt::println("start");
            timer.expires_after(std::chrono::seconds{1});
            co_await timer.async_wait(boost::asio::use_awaitable);

            co_await liquid_dispenser.dispense(500 * mp_units::si::milli<mp_units::si::litre>);
        },
        boost::asio::detached);

    io.run();
    return 0;
}
