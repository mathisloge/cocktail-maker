#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <catch2/catch_test_macros.hpp>
#include <exec/asio/use_sender.hpp>
#include <stdexec/execution.hpp>
import std;
import cm.core;

using namespace cm;
using namespace std::chrono_literals;

namespace {
Task<void> delay(boost::asio::io_context& ioc, std::chrono::milliseconds duration)
{
    boost::asio::steady_timer timer{ioc, duration};
    co_await timer.async_wait(exec::asio::use_sender);
}

class TestError : public std::runtime_error
{
  public:
    using runtime_error::runtime_error;
};
} // namespace

TEST_CASE("join_all - runs all tasks concurrently", "[async_algorithms][join_all]")
{
    boost::asio::io_context ioc;
    int finished = 0;

    auto worker = [&](std::chrono::milliseconds duration) -> Task<void> {
        co_await delay(ioc, duration);
        ++finished;
    };

    std::vector<Task<void>> tasks;
    tasks.push_back(worker(20ms));
    tasks.push_back(worker(10ms));
    tasks.push_back(worker(0ms));

    REQUIRE(run_until_complete(ioc, join_all(std::move(tasks))));
    REQUIRE(finished == 3);
}

TEST_CASE("join_all - an empty range completes right away", "[async_algorithms][join_all]")
{
    boost::asio::io_context ioc;
    REQUIRE(run_until_complete(ioc, join_all({})));
}

TEST_CASE("join_all - the first error stops the siblings and is rethrown", "[async_algorithms][join_all]")
{
    boost::asio::io_context ioc;
    bool sibling_finished = false;

    auto failing = [&]() -> Task<void> {
        co_await delay(ioc, 1ms);
        throw TestError{"boom"};
    };
    auto long_running = [&]() -> Task<void> {
        co_await delay(ioc, 10s);
        sibling_finished = true;
    };

    std::vector<Task<void>> tasks;
    tasks.push_back(long_running());
    tasks.push_back(failing());

    const auto start = std::chrono::steady_clock::now();
    REQUIRE_THROWS_AS(run_until_complete(ioc, join_all(std::move(tasks))), TestError);
    REQUIRE_FALSE(sibling_finished);
    REQUIRE(std::chrono::steady_clock::now() - start < 5s);
}

TEST_CASE("gather_all - lets all tasks finish although one fails", "[async_algorithms][gather_all]")
{
    boost::asio::io_context ioc;
    bool sibling_finished = false;

    auto failing = [&]() -> Task<void> {
        co_await delay(ioc, 1ms);
        throw TestError{"boom"};
    };
    auto slower = [&]() -> Task<void> {
        co_await delay(ioc, 20ms);
        sibling_finished = true;
    };

    std::vector<Task<void>> tasks;
    tasks.push_back(slower());
    tasks.push_back(failing());

    REQUIRE_THROWS_AS(run_until_complete(ioc, gather_all(std::move(tasks))), TestError);
    REQUIRE(sibling_finished);
}

TEST_CASE("gather_all - a stop request of the caller reaches every task", "[async_algorithms][gather_all]")
{
    boost::asio::io_context ioc;
    stdexec::inplace_stop_source stop_source;
    int finished = 0;

    auto long_running = [&]() -> Task<void> {
        co_await delay(ioc, 10s);
        ++finished;
    };
    auto stopper = [&]() -> Task<void> {
        co_await delay(ioc, 1ms);
        stop_source.request_stop();
    };

    std::vector<Task<void>> tasks;
    tasks.push_back(long_running());
    tasks.push_back(long_running());

    auto gathered =
        stdexec::write_env(gather_all(std::move(tasks)), stdexec::prop{stdexec::get_stop_token, stop_source.get_token()});
    REQUIRE_FALSE(run_until_complete(ioc, stdexec::when_all(std::move(gathered), stopper())));
    REQUIRE(finished == 0);
}

TEST_CASE("AsyncScope - spawned work runs and join waits for it", "[async_scope]")
{
    boost::asio::io_context ioc;
    AsyncScope scope{ioc};
    int finished = 0;

    auto worker = [&]() -> Task<void> {
        co_await delay(ioc, 5ms);
        ++finished;
    };
    auto failing = [&]() -> Task<void> {
        co_await delay(ioc, 1ms);
        throw TestError{"logged, not propagated"};
    };

    scope.spawn(worker());
    scope.spawn(failing());
    scope.spawn(worker());

    REQUIRE(run_until_complete(ioc, scope.join()));
    REQUIRE(finished == 2);
}

TEST_CASE("AsyncScope - request_stop stops the spawned work", "[async_scope]")
{
    boost::asio::io_context ioc;
    AsyncScope scope{ioc};
    bool finished = false;

    auto long_running = [&]() -> Task<void> {
        co_await delay(ioc, 10s);
        finished = true;
    };

    scope.spawn(long_running());
    auto stop_then_join = [&]() -> Task<void> {
        scope.request_stop();
        co_await scope.join();
    };

    REQUIRE(run_until_complete(ioc, stop_then_join()));
    REQUIRE_FALSE(finished);
}

TEST_CASE("Channel - values are received in order and a stop request ends the receive", "[channel]")
{
    boost::asio::io_context ioc;
    Channel<int> channel{ioc, 2};
    std::vector<int> received;

    auto producer = [&]() -> Task<void> {
        co_await async_send(channel, 1);
        co_await async_send(channel, 2);
    };
    auto consumer = [&]() -> Task<void> {
        received.push_back(co_await async_receive(channel));
        received.push_back(co_await async_receive(channel));
    };

    REQUIRE(run_until_complete(ioc, stdexec::when_all(producer(), consumer())));
    REQUIRE(received == std::vector{1, 2});

    stdexec::inplace_stop_source stop_source;
    auto waiting_consumer = [&]() -> Task<void> { received.push_back(co_await async_receive(channel)); };
    auto stopper = [&]() -> Task<void> {
        co_await delay(ioc, 1ms);
        stop_source.request_stop();
    };
    auto stoppable_consumer =
        stdexec::write_env(waiting_consumer(), stdexec::prop{stdexec::get_stop_token, stop_source.get_token()});
    REQUIRE_FALSE(run_until_complete(ioc, stdexec::when_all(std::move(stoppable_consumer), stopper())));
    REQUIRE(received.size() == 2);
}
