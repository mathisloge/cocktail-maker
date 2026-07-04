#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt.hpp>
#include <catch2/catch_test_macros.hpp>
import cm.core;

using namespace cm;
namespace cobalt = boost::cobalt;

TEST_CASE("AwaitableBool – already true, immediate sequel", "[awaitable_bool]")
{
    cobalt::run([]() -> cobalt::task<void> {
        AwaitableBool flag{co_await cobalt::this_coro::executor};
        flag = true;

        bool reached = false;
        co_await flag; // await_ready() == true → no suspend
        reached = true;

        REQUIRE(reached);
        REQUIRE(static_cast<bool>(flag));
    }());
}

TEST_CASE("AwaitableBool – Waiter is woken up after set.", "[awaitable_bool]")
{
    cobalt::run([]() -> cobalt::task<void> {
        AwaitableBool flag{co_await cobalt::this_coro::executor};
        bool waiter_resumed = false;

        auto waiter = [&]() -> cobalt::task<void> {
            co_await flag;
            waiter_resumed = true;
        };

        auto setter = [&]() -> cobalt::task<void> {
            flag = true;
            co_return;
        };

        co_await cobalt::join(waiter(), setter());
        REQUIRE(waiter_resumed);
    }());
}

TEST_CASE("AwaitableBool – multiple waiters will all be woken up.", "[awaitable_bool]")
{
    cobalt::run([]() -> cobalt::task<void> {
        AwaitableBool flag{co_await cobalt::this_coro::executor};
        int resume_count = 0;

        auto make_waiter = [&]() -> cobalt::task<void> {
            co_await flag;
            ++resume_count;
        };

        auto setter = [&]() -> cobalt::task<void> {
            flag = true;
            co_return;
        };

        co_await cobalt::join(make_waiter(), make_waiter(), make_waiter(), setter());
        REQUIRE(resume_count == 3);
    }());
}

TEST_CASE("AwaitableBool – a double set(true) is idempotent", "[awaitable_bool]")
{
    cobalt::run([]() -> cobalt::task<void> {
        AwaitableBool flag{co_await cobalt::this_coro::executor};
        int resume_count = 0;

        auto waiter = [&]() -> cobalt::task<void> {
            co_await flag;
            ++resume_count;
        };

        auto setter = [&]() -> cobalt::task<void> {
            flag = true;
            flag = true; // set again
            co_return;
        };

        co_await cobalt::join(waiter(), setter());
        REQUIRE(resume_count == 1);
    }());
}

TEST_CASE("AwaitableBool – after a reset, it can be awaited again.", "[awaitable_bool]")
{
    cobalt::run([]() -> cobalt::task<void> {
        AwaitableBool flag{co_await cobalt::this_coro::executor};

        flag = true;
        flag = false;
        REQUIRE_FALSE(static_cast<bool>(flag));

        bool resumed = false;

        auto waiter = [&]() -> cobalt::task<void> {
            co_await flag;
            resumed = true;
        };

        auto setter = [&]() -> cobalt::task<void> {
            flag = true;
            co_return;
        };

        co_await cobalt::join(waiter(), setter());
        REQUIRE(resumed);
    }());
}
