// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/execution_context.hpp"

namespace cm {

ExecutionContext::ExecutionContext(boost::asio::any_io_executor executor, std::unique_ptr<WeightSensor> weight_sensor)
    : io_context_{executor}
    , resume_timer_{executor}
    , weight_sensor_{std::move(weight_sensor)}
{
}

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

EventBus& ExecutionContext::event_bus()
{
    return event_bus_;
}

LiquidDispenserRegistry& ExecutionContext::liquid_registry()
{
    return liquid_registry_;
}

WeightSensor& ExecutionContext::weight_sensor()
{
    return *weight_sensor_;
}

boost::asio::any_io_executor ExecutionContext::async_executor()
{
    return io_context_;
}
} // namespace cm
