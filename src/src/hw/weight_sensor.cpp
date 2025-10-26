// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/hw/weight_sensor.hpp"
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <spdlog/spdlog.h>
#include "cm/logging.hpp"

namespace cm {
WeightSensor::WeightSensor(std::string name)
    : logger_{LoggingContext::instance().create_logger(std::move(name))}
{
}

WeightSensor::~WeightSensor() = default;

void WeightSensor::start_measure_loop(const boost::asio::any_io_executor& io, boost::asio::cancellation_slot stoken)
{
    boost::asio::co_spawn(
        io,
        [self = shared_from_this()] -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};
            SPDLOG_LOGGER_DEBUG(self->logger_, "Starting load cell measure loop...");
            while (true) {
                self->measure_ = MeasurePoint{
                    .weight = co_await self->read(),
                    .time = std::chrono::system_clock::now(),
                };
                SPDLOG_LOGGER_TRACE(logger_, "Measured weight: {}", measure_.weight.quantity_from_zero());
                timer.expires_after(std::chrono::milliseconds{100});
                co_await timer.async_wait(boost::asio::use_awaitable);
            }
            co_return;
        },
        boost::asio::bind_cancellation_slot(std::move(stoken), boost::asio::detached));
}

WeightSensor::MeasurePoint WeightSensor::last_measure() const
{
    return measure_;
}
} // namespace cm
