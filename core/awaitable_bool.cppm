module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>

export module cm.core:awaitable_bool;
import std;

namespace asio = boost::asio;

namespace cm {
export class AwaitableBool
{
  public:
    explicit AwaitableBool(asio::any_io_executor exec)
        : exec_(std::move(exec))
    {
    }

    AwaitableBool(const AwaitableBool&) = delete ("Prevent dangling refs in Awaiter");
    AwaitableBool& operator=(const AwaitableBool&) = delete;

    AwaitableBool& operator=(bool v)
    {
        if (v && !value_) {
            // false → true: wake all
            value_ = true;
            auto ws = std::exchange(waiters_, {});
            for (auto h : ws) {
                asio::post(exec_, [h]() mutable { h.resume(); });
            }
        }
        else {
            // true → false: reset, new waiters will come.
            value_ = v;
        }
        return *this;
    }

    explicit operator bool() const noexcept
    {
        return value_;
    }

    auto operator co_await() noexcept
    {
        struct Awaiter
        {
            AwaitableBool& self;

            // Schon true? → direkt fortsetzen, kein Suspend
            bool await_ready() const noexcept
            {
                return self.value_;
            }

            void await_suspend(std::coroutine_handle<> h)
            {
                self.waiters_.push_back(h);
            }

            void await_resume() const noexcept
            {
            }
        };

        return Awaiter{*this};
    }

  private:
    bool value_ = false;
    asio::any_io_executor exec_;
    std::vector<std::coroutine_handle<>> waiters_;
};

} // namespace cm
