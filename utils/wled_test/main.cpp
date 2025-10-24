// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <boost/asio.hpp>
#include <cm/hw/wled_serial.hpp>
#include <cm/scoped_action.hpp>
#include <fmt/core.h>

int main()
{
    boost::asio::io_context io;

    auto wled = cm::WledSerial::create(io.get_executor(), "/dev/ttyUSB0", {{0, 24}});
    boost::asio::co_spawn(
        io,
        [wled] -> boost::asio::awaitable<void> {
            cm::ScopedAction shutdown{[wled] { wled->shutdown(); }};
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};
            fmt::println("start");
            co_await wled->turn_on();
            fmt::println("turned on");

            timer.expires_after(std::chrono::seconds{1});
            co_await timer.async_wait(boost::asio::use_awaitable);

            wled->set_state(0, cm::WledSerial::State::active);

            timer.expires_after(std::chrono::seconds{5});
            co_await timer.async_wait(boost::asio::use_awaitable);

            wled->set_state(0, cm::WledSerial::State::empty_ingredient);

            timer.expires_after(std::chrono::seconds{5});
            co_await timer.async_wait(boost::asio::use_awaitable);

            wled->set_state(0, cm::WledSerial::State::inactive);

            timer.expires_after(std::chrono::seconds{5});
            co_await timer.async_wait(boost::asio::use_awaitable);

            wled->set_state(0, cm::WledSerial::State::none);
            co_await wled->turn_off();

            fmt::println("turned off");
            fmt::println("finished");
            co_return;
        },
        boost::asio::detached);

    io.run();
    return 0;
}
