#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <exec/when_any.hpp>
#include <stdexec/execution.hpp>
import std;
import cm.core;

using namespace std::chrono_literals;

TEST_CASE("async_sleep waits at least its duration", "[sleep]")
{
    boost::asio::io_context ioc;
    const auto start = std::chrono::steady_clock::now();

    REQUIRE(cm::run_until_complete(ioc, cm::async_sleep(ioc.get_executor(), 20ms)));

    CHECK(std::chrono::steady_clock::now() - start >= 20ms);
}

TEST_CASE("wait_until_stopped ends only through a stop request", "[sleep]")
{
    boost::asio::io_context ioc;
    bool stopped = false;

    REQUIRE(cm::run_until_complete(
        ioc,
        exec::when_any(cm::wait_until_stopped(ioc.get_executor()) | stdexec::upon_stopped([&]() noexcept { stopped = true; }),
                       cm::async_sleep(ioc.get_executor(), 10ms))));

    CHECK(stopped);
}

TEST_CASE("AsyncScope::stop_and_join stops and waits for spawned work", "[async_scope]")
{
    boost::asio::io_context ioc;
    cm::AsyncScope scope{ioc};
    bool stopped = false;
    scope.spawn(cm::wait_until_stopped(ioc.get_executor()) | stdexec::upon_stopped([&]() noexcept { stopped = true; }));

    REQUIRE(cm::run_until_complete(ioc, scope.stop_and_join()));

    CHECK(stopped);
}
