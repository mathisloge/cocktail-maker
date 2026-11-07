module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <exec/asio/use_sender.hpp>

module cm.core:sleep_impl;
import std;
import :sleep;

namespace cm {
Task<void> async_sleep(boost::asio::any_io_executor executor, const std::chrono::steady_clock::duration duration)
{
    boost::asio::steady_timer timer{std::move(executor), duration};
    co_await timer.async_wait(exec::asio::use_sender);
}

Task<void> wait_until_stopped(boost::asio::any_io_executor executor)
{
    boost::asio::steady_timer never{std::move(executor), boost::asio::steady_timer::time_point::max()};
    co_await never.async_wait(exec::asio::use_sender);
}
} // namespace cm
