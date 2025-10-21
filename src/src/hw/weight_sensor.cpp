#include "cm/hw/weight_sensor.hpp"
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <spdlog/spdlog.h>
#include "cm/logging.hpp"

namespace cm {
WeightSensor::WeightSensor(std::string name, const boost::asio::any_io_executor& io)
    : logger_{LoggingContext::instance().create_logger(std::move(name))}
{
    boost::asio::co_spawn(
        io,
        [this] -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};
            SPDLOG_LOGGER_DEBUG(logger_, "Starting load cell measure loop...");
            while (true) {
                measure_ = MeasurePoint{
                    .weight = co_await read(),
                    .time = std::chrono::system_clock::now(),
                };
                SPDLOG_LOGGER_DEBUG(logger_, "Measured weight: {}", measure_.weight.quantity_from_zero());
                timer.expires_after(std::chrono::milliseconds{100});
                co_await timer.async_wait(boost::asio::use_awaitable);
            }
            co_return;
        },
        boost::asio::bind_cancellation_slot(cancel_signal_.slot(), boost::asio::detached));
}

WeightSensor::~WeightSensor()
{
    cancel_signal_.emit(boost::asio::cancellation_type::all);
}

WeightSensor::MeasurePoint WeightSensor::last_measure() const
{
    return measure_;
}
} // namespace cm
