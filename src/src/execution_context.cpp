#include "cm/execution_context.hpp"

namespace cm
{

ExecutionContext::ExecutionContext(boost::asio::io_context &io)
    : io_context_{io}
    , resume_timer_{io}
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

boost::asio::io_context &ExecutionContext::io_context()
{
    return io_context_;
}
} // namespace cm
