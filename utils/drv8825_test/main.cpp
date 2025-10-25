// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <boost/asio.hpp>
#include <cm/hw/drv8825_stepper_moter.hpp>
#include <fmt/core.h>
#include <thread>

using namespace mp_units::si::unit_symbols;

int main()
{
    boost::asio::thread_pool thread_pool{1};
    cm::Drv8825StepperMotorDriver stepper1{cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {22}},
                                           cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {27}},
                                           cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {17}}};

    cm::Drv8825StepperMotorDriver stepper2{cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {25}},
                                           cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {24}},
                                           cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {23}}};

    cm::Drv8825StepperMotorDriver stepper3{cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {16}},
                                           cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {20}},
                                           cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {21}}};
    boost::asio::co_spawn(
        thread_pool,
        [&] -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};

            fmt::println("start1");
            co_await stepper1.enable();
            timer.expires_after(std::chrono::seconds{1});
            co_await timer.async_wait(boost::asio::use_awaitable);

            co_await stepper1.step(3000 * cm::units::step, (500 * cm::units::step) / s);

            co_await stepper1.disable();
            co_return;
        },
        boost::asio::detached);

    boost::asio::co_spawn(
        thread_pool,
        [&] -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};

            fmt::println("start2");
            co_await stepper2.enable();
            timer.expires_after(std::chrono::seconds{1});
            co_await timer.async_wait(boost::asio::use_awaitable);

            co_await stepper2.step(3000 * cm::units::step, (500 * cm::units::step) / s);

            co_await stepper2.disable();
            co_return;
        },
        boost::asio::detached);

    boost::asio::co_spawn(
        thread_pool,
        [&] -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};

            fmt::println("start3");
            co_await stepper3.enable();
            timer.expires_after(std::chrono::seconds{1});
            co_await timer.async_wait(boost::asio::use_awaitable);

            co_await stepper3.step(3000 * cm::units::step, (500 * cm::units::step) / s);

            co_await stepper3.disable();
            co_return;
        },
        boost::asio::detached);

    thread_pool.join();
    thread_pool.wait();
    return 0;
}
