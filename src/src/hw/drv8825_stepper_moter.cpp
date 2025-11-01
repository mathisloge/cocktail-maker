// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/hw/drv8825_stepper_moter.hpp"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <libassert/assert.hpp>
#include <mp-units/math.h>
#include <spdlog/spdlog.h>
#include "cm/logging.hpp"

namespace asio = boost::asio;

namespace cm {
using SecondsPerStep = units::quantity<units::si::second / units::step>;
constexpr std::chrono::microseconds kPulseWidth{10};

namespace {
template <typename T>
auto create_line(const std::string& name, PinSelection<T> pin, spdlog::logger& logger) -> std::optional<gpiod::line_request>
{
    try {
        return gpiod::chip{std::move(pin.chip)}
            .prepare_request()
            .set_consumer(name)
            .add_line_settings(pin.offset, gpiod::line_settings{}.set_direction(gpiod::line::direction::OUTPUT))
            .do_request();
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_CRITICAL(&logger, "Could not initialize stepper motor! Error: {}", ex.what());
    }
    return std::nullopt;
}
} // namespace

Drv8825StepperMotorDriver::Drv8825StepperMotorDriver(Drv8825EnablePin enable_pin,
                                                     Drv8825StepPin step_pin,
                                                     Drv8825DirectionPin direction_pin)
    : logger_{LoggingContext::instance().create_logger(fmt::format("stepper@e{}d{}s{}",
                                                                   static_cast<int>(enable_pin.offset),
                                                                   static_cast<int>(step_pin.offset),
                                                                   static_cast<int>(direction_pin.offset)))}
    , enable_line_{create_line("drv8825-enable", enable_pin, *logger_)}
    , enable_offset_{enable_pin.offset}
    , direction_line_{create_line("drv8825-direction", direction_pin, *logger_)}
    , direction_offset_{direction_pin.offset}
    , step_line_{create_line("drv8825-step", step_pin, *logger_)}
    , step_offset_{step_pin.offset}
{
    if (enable_line_.has_value()) {
        try {
            // disable at start
            enable_line_->set_value(enable_offset_, gpiod::line::value::ACTIVE);
        }
        catch (const std::exception& ex) {
            SPDLOG_LOGGER_ERROR(logger_, "Could not disable stepper motor! Error: {}", ex.what());
        }
    }
    else {
        SPDLOG_LOGGER_DEBUG(logger_, "enable line wasn't initialized. Cannot disable line.");
    }
}

boost::asio::awaitable<void> Drv8825StepperMotorDriver::enable()
{
    ASSERT(enable_line_.has_value());
    auto exec = co_await asio::this_coro::executor;
    enable_line_->set_value(enable_offset_, gpiod::line::value::INACTIVE);

    asio::steady_timer timer{exec};
    timer.expires_after(std::chrono::milliseconds{10});
    co_await timer.async_wait(asio::use_awaitable);
}

boost::asio::awaitable<void> Drv8825StepperMotorDriver::disable()
{
    ASSERT(enable_line_.has_value());
    auto exec = co_await asio::this_coro::executor;
    enable_line_->set_value(enable_offset_, gpiod::line::value::ACTIVE);

    asio::steady_timer timer{exec};
    timer.expires_after(std::chrono::milliseconds{10});
    co_await timer.async_wait(asio::use_awaitable);
}

boost::asio::awaitable<void> Drv8825StepperMotorDriver::step(units::Steps steps, units::StepsPerSecond velocity)
{
    ASSERT(direction_line_.has_value());
    auto exec = co_await asio::this_coro::executor;
    asio::steady_timer timer{exec};

    const auto direction = steps > 0 * units::step ? gpiod::line::value::ACTIVE : gpiod::line::value::INACTIVE;
    if (steps < 0 * units::step) {
        steps *= -1;
    }
    direction_line_->set_value(direction_offset_, direction);
    timer.expires_after(kPulseWidth);
    co_await timer.async_wait(asio::use_awaitable);

    // 1. Compute ramp time
    const auto ramp_time = velocity / kAcceleration;

    // 2. Compute ramp distance (steps) with: s = 0.5 * a * t^2
    const auto steps_double = (0.5 * kAcceleration * mp_units::pow<2>(ramp_time)).numerical_value_in(units::step);
    units::Steps ramp_steps = static_cast<std::int32_t>(std::floor(steps_double)) * units::step;

    if ((2 * ramp_steps) > steps) {
        ramp_steps = steps / 2; // triangle profile
    }

    const units::Steps cruise_steps = steps - (2 * ramp_steps);

    // 3. Acceleration phase
    for (units::Steps i = 1 * units::step; i <= ramp_steps; i += 1 * units::step) {
        const SecondsPerStep delay = 1 / mp_units::sqrt(2.0 * kAcceleration * i); // v = √(2a * s)
        co_await step_one(std::chrono::duration_cast<std::chrono::microseconds>(units::to_chrono_duration(delay)));
    }

    // 4. Cruise phase
    const SecondsPerStep cruise_delay = 1.0 / velocity; // delay = 1/velocity gives seconds per step
    for (units::Steps i = 0 * units::step; i < cruise_steps; i += 1 * units::step) {
        co_await step_one(std::chrono::duration_cast<std::chrono::microseconds>(mp_units::to_chrono_duration(cruise_delay)));
    }

    // 5. Deceleration phase
    for (units::Steps i = ramp_steps; i >= 1 * units::step; i -= 1 * units::step) {
        const SecondsPerStep delay = 1 / mp_units::sqrt(2.0 * kAcceleration * i); // v = √(2a * s)
        co_await step_one(std::chrono::duration_cast<std::chrono::microseconds>(mp_units::to_chrono_duration(delay)));
    }
}

async<void> Drv8825StepperMotorDriver::step_one(std::chrono::microseconds wait_after)
{
    ASSERT(step_line_.has_value());

    auto exec = co_await asio::this_coro::executor;
    asio::steady_timer timer{exec};

    step_line_->set_value(step_offset_, gpiod::line::value::ACTIVE);
    timer.expires_after(kPulseWidth);
    co_await timer.async_wait(asio::use_awaitable);
    step_line_->set_value(step_offset_, gpiod::line::value::INACTIVE);

    timer.expires_after(wait_after);
    co_await timer.async_wait(asio::use_awaitable);
}

} // namespace cm
