#include "cm/execution_context.hpp"

namespace cm
{

ExecutionContext::ExecutionContext(boost::asio::any_io_executor executor)
    : io_context_{executor}
    , resume_timer_{executor}
{}

boost::asio::awaitable<void> ExecutionContext::wait_for_resume()
{
    resume_timer_.expires_at(std::chrono::steady_clock::time_point::max());
    boost::system::error_code ec;
    co_await resume_timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return;
}

void ExecutionContext::resume()
{
    resume_timer_.cancel(); // wakes up `wait_for_resume()`
}

EventBus &ExecutionContext::event_bus()
{
    return event_bus_;
}

LiquidDispenserRegistry &ExecutionContext::liquid_registry()
{
    return liquid_registry_;
}

boost::asio::any_io_executor ExecutionContext::async_executor()
{
    return io_context_;
}
} // namespace cm
