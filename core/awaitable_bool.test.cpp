#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexec/execution.hpp>
import std;
import cm.core;

using namespace cm;

TEST_CASE("AwaitableBool - already true, immediate sequel", "[awaitable_bool]")
{
    boost::asio::io_context ioc;
    AwaitableBool flag{ioc.get_executor()};
    flag = true;

    bool reached = false;
    auto test = [&]() -> Task<void> {
        co_await flag.async_wait();
        reached = true;
    };

    REQUIRE(run_until_complete(ioc, test()));
    REQUIRE(reached);
    REQUIRE(static_cast<bool>(flag));
}

TEST_CASE("AwaitableBool - Waiter is woken up after set.", "[awaitable_bool]")
{
    boost::asio::io_context ioc;
    AwaitableBool flag{ioc.get_executor()};
    bool waiter_resumed = false;

    auto waiter = [&]() -> Task<void> {
        co_await flag.async_wait();
        waiter_resumed = true;
    };

    auto setter = [&]() -> Task<void> {
        flag = true;
        co_return;
    };

    REQUIRE(run_until_complete(ioc, stdexec::when_all(waiter(), setter())));
    REQUIRE(waiter_resumed);
}

TEST_CASE("AwaitableBool - multiple waiters will all be woken up.", "[awaitable_bool]")
{
    boost::asio::io_context ioc;
    AwaitableBool flag{ioc.get_executor()};
    int resume_count = 0;

    auto make_waiter = [&]() -> Task<void> {
        co_await flag.async_wait();
        ++resume_count;
    };

    auto setter = [&]() -> Task<void> {
        flag = true;
        co_return;
    };

    REQUIRE(run_until_complete(ioc, stdexec::when_all(make_waiter(), make_waiter(), make_waiter(), setter())));
    REQUIRE(resume_count == 3);
}

TEST_CASE("AwaitableBool - a double set(true) is idempotent", "[awaitable_bool]")
{
    boost::asio::io_context ioc;
    AwaitableBool flag{ioc.get_executor()};
    int resume_count = 0;

    auto waiter = [&]() -> Task<void> {
        co_await flag.async_wait();
        ++resume_count;
    };

    auto setter = [&]() -> Task<void> {
        flag = true;
        flag = true; // set again
        co_return;
    };

    REQUIRE(run_until_complete(ioc, stdexec::when_all(waiter(), setter())));
    REQUIRE(resume_count == 1);
}

TEST_CASE("AwaitableBool - after a reset, it can be awaited again.", "[awaitable_bool]")
{
    boost::asio::io_context ioc;
    AwaitableBool flag{ioc.get_executor()};

    flag = true;
    flag = false;
    REQUIRE_FALSE(static_cast<bool>(flag));

    bool resumed = false;

    auto waiter = [&]() -> Task<void> {
        co_await flag.async_wait();
        resumed = true;
    };

    auto setter = [&]() -> Task<void> {
        flag = true;
        co_return;
    };

    REQUIRE(run_until_complete(ioc, stdexec::when_all(waiter(), setter())));
    REQUIRE(resumed);
}

TEST_CASE("AwaitableBool - a stop request completes a waiter with stopped.", "[awaitable_bool]")
{
    boost::asio::io_context ioc;
    AwaitableBool flag{ioc.get_executor()};
    stdexec::inplace_stop_source stop_source;
    bool resumed = false;

    auto waiter = [&]() -> Task<void> {
        co_await flag.async_wait();
        resumed = true;
    };

    auto stopper = [&]() -> Task<void> {
        stop_source.request_stop();
        co_return;
    };

    auto waiter_with_stop = stdexec::write_env(waiter(), stdexec::prop{stdexec::get_stop_token, stop_source.get_token()});
    const bool completed = run_until_complete(ioc, stdexec::when_all(std::move(waiter_with_stop), stopper()));

    REQUIRE_FALSE(completed);
    REQUIRE_FALSE(resumed);
    REQUIRE_FALSE(static_cast<bool>(flag));
}
