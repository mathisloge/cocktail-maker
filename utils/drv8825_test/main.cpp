// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <thread>
#include <boost/asio.hpp>
#include <cm/hw/drv8825_stepper_moter.hpp>
#include <fmt/core.h>

using namespace mp_units::si::unit_symbols;

int main()
{
    boost::asio::io_context io;
    cm::Drv8825StepperMotorDriver stepper{cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {24}},
                                          cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {23}},
                                          cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {23}}};

    boost::asio::co_spawn(
        io,
        [&stepper] -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};

            fmt::println("start");
            timer.expires_after(std::chrono::seconds{1});
            co_await timer.async_wait(boost::asio::use_awaitable);

            co_await stepper.step(500 * cm::units::step, (2 * cm::units::step) / s);

            co_return;
        },
        boost::asio::detached);

    io.run();
    return 0;
}
