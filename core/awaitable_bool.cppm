module;
#include <boost/asio/any_io_executor.hpp>

export module cm.core:awaitable_bool;
import std;

namespace cm {

export class AwaitableBool
{
  public:
    struct Awaiter
    {
        AwaitableBool& self;

        [[nodiscard]] bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> h);
        void await_resume() const noexcept;
    };

    explicit AwaitableBool(boost::asio::any_io_executor exec);

    AwaitableBool(const AwaitableBool&) = delete ("Prevent dangling refs in Awaiter");
    AwaitableBool& operator=(const AwaitableBool&) = delete;

    AwaitableBool& operator=(bool v);

    explicit operator bool() const noexcept;

    Awaiter operator co_await() noexcept;

  private:
    bool value_ = false;
    boost::asio::any_io_executor exec_;
    std::vector<std::coroutine_handle<>> waiters_;
};

} // namespace cm
