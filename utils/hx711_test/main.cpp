// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <boost/asio.hpp>
#include <cm/hw/hx711_sensor.hpp>
#include <fmt/core.h>

using namespace mp_units::si::unit_symbols;

int main()
{
    boost::asio::io_context io;
    cm::Hx711Sensor load_cell{io.get_executor(),
                              cm::Hx711DatPin{.chip = "/dev/gpiochip0", .offset = {24}},
                              cm::Hx711ClkPin{.chip = "/dev/gpiochip0", .offset = {23}}};

    boost::asio::co_spawn(
        io,
        [&load_cell] -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};

            fmt::println("start");
            timer.expires_after(std::chrono::seconds{1});
            co_await timer.async_wait(boost::asio::use_awaitable);
            co_await load_cell.tare();

            fmt::println("place ref weight");
            timer.expires_after(std::chrono::seconds{5});
            co_await timer.async_wait(boost::asio::use_awaitable);
            co_await load_cell.calibrate_with_ref_weight(100 * g);

            while (true) {
                fmt::println("read: {}", load_cell.last_measure().weight.quantity_from_zero());
                timer.expires_after(std::chrono::milliseconds{500});
                co_await timer.async_wait(boost::asio::use_awaitable);
            }
            co_return;
        },
        boost::asio::detached);

    io.run();
    return 0;
}
