module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>

module cm.core:awaitable_bool_impl;
import std;
import :awaitable_bool;

namespace cm {

AwaitableBool::AwaitableBool(boost::asio::any_io_executor exec)
    : exec_(std::move(exec))
{
}

AwaitableBool& AwaitableBool::operator=(bool v)
{
    if (v && !value_) {
        // false → true: wake all
        value_ = true;
        auto ws = std::exchange(waiters_, {});
        for (auto h : ws) {
            boost::asio::post(exec_, [h]() mutable { h.resume(); });
        }
    }
    else {
        // true → false: reset, new waiters will come.
        value_ = v;
    }
    return *this;
}

AwaitableBool::operator bool() const noexcept
{
    return value_;
}

auto AwaitableBool::operator co_await() noexcept -> AwaitableBool::Awaiter
{
    return Awaiter{*this};
}

bool AwaitableBool::Awaiter::await_ready() const noexcept
{
    return self.value_;
}

void AwaitableBool::Awaiter::await_suspend(std::coroutine_handle<> h)
{
    self.waiters_.push_back(h);
}

void AwaitableBool::Awaiter::await_resume() const noexcept
{
}

} // namespace cm
