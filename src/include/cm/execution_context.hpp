#pragma once
#include <boost/asio.hpp>
#include "execution_event_sink.hpp"

namespace cm
{
class ExecutionContext
{
  public:
    explicit ExecutionContext(boost::asio::io_context &io, std::unique_ptr<ExecutionEventSink> event_sink)
        : io_context_{io}
        , event_sink_{std::move(event_sink)}
        , resume_timer_{io}
    {}

    boost::asio::awaitable<void> wait_for_resume()
    {
        resume_timer_.expires_at(std::chrono::steady_clock::time_point::max());
        boost::system::error_code ec;
        co_await resume_timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        co_return;
    }

    void resume()
    {
        resume_timer_.cancel(); // wakes up `wait_for_resume()`
    }

  private:
    boost::asio::io_context &io_context_;
    std::unique_ptr<ExecutionEventSink> event_sink_;
    boost::asio::steady_timer resume_timer_;
};
} // namespace cm
