#include "cm/hw/drv8825_stepper_moter.hpp"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <mp-units/math.h>

namespace async = boost::asio;
namespace cm
{
using SecondsPerStep = decltype(1. * mp_units::si::second / 1. * units::step);
constexpr std::chrono::microseconds kPulseWidth{10};

Drv8825StepperMotorDriver::Drv8825StepperMotorDriver(Drv8825EnablePin enable_pin,
                                                     Drv8825StepPin step_pin,
                                                     Drv8825DirectionPin direction_pin)
    : enable_line_{gpiod::chip{std::move(enable_pin.chip)}
                       .prepare_request()
                       .set_consumer("drv8825-enable")
                       .add_line_settings(enable_pin.offset,
                                          gpiod::line_settings{}.set_direction(gpiod::line::direction::OUTPUT))
                       .do_request()}
    , enable_offset_{enable_pin.offset}
    , step_line_{gpiod::chip{std::move(step_pin.chip)}
                     .prepare_request()
                     .set_consumer("drv8825-step")
                     .add_line_settings(step_pin.offset,
                                        gpiod::line_settings{}.set_direction(gpiod::line::direction::OUTPUT))
                     .do_request()}
    , step_offset_{step_pin.offset}
    , direction_line_{gpiod::chip{std::move(direction_pin.chip)}
                          .prepare_request()
                          .set_consumer("drv8825-direction")
                          .add_line_settings(direction_pin.offset,
                                             gpiod::line_settings{}.set_direction(gpiod::line::direction::OUTPUT))
                          .do_request()}
    , direction_offset_{direction_pin.offset}
{
    // disable at start
    enable_line_.set_value(enable_offset_, gpiod::line::value::ACTIVE);
}

boost::asio::awaitable<void> Drv8825StepperMotorDriver::enable()
{
    auto exec = co_await async::this_coro::executor;
    enable_line_.set_value(enable_offset_, gpiod::line::value::INACTIVE);

    async::steady_timer timer{exec};
    timer.expires_after(std::chrono::milliseconds{10});
    co_await timer.async_wait(async::use_awaitable);
}

boost::asio::awaitable<void> Drv8825StepperMotorDriver::disable()
{
    auto exec = co_await async::this_coro::executor;
    enable_line_.set_value(enable_offset_, gpiod::line::value::ACTIVE);

    async::steady_timer timer{exec};
    timer.expires_after(std::chrono::milliseconds{10});
    co_await timer.async_wait(async::use_awaitable);
}

boost::asio::awaitable<void> Drv8825StepperMotorDriver::step(units::Steps steps, units::StepsPerSecond velocity)
{
    auto exec = co_await async::this_coro::executor;
    async::steady_timer timer{exec};

    const auto direction = steps > 0 ? gpiod::line::value::ACTIVE : gpiod::line::value::INACTIVE;
    if (steps < 0)
    {
        steps *= -1;
    }
    direction_line_.set_value(direction_offset_, direction);
    timer.expires_after(kPulseWidth);
    co_await timer.async_wait(async::use_awaitable);

    // 1. Compute ramp time
    const auto ramp_time = velocity / kAcceleration;

    // 2. Compute ramp distance (steps) with: s = 0.5 * a * t^2
    auto steps_double = (0.5 * kAcceleration * mp_units::pow<2>(ramp_time)).numerical_value_in(units::step);
    units::Steps ramp_steps = units::Steps{static_cast<std::int32_t>(std::floor(steps_double))};

    if ((2 * ramp_steps) > steps)
    {
        ramp_steps = steps / 2; // triangle profile
    }

    const units::Steps cruise_steps = steps - (2 * ramp_steps);

    // 3. Acceleration phase
    for (units::Steps i = 1 * units::step; i <= ramp_steps; i += 1 * units::step)
    {
        const SecondsPerStep delay = 1 / mp_units::sqrt(2.0 * kAcceleration * i); // v = √(2a * s)
        co_await step_one(timer,
                          std::chrono::duration_cast<std::chrono::microseconds>(mp_units::to_chrono_duration(delay)));
    }

    // 4. Cruise phase
    const SecondsPerStep cruise_delay = 1.0 / velocity; // delay = 1/velocity gives seconds per step
    for (units::Steps i = 0 * units::step; i < cruise_steps; i += 1 * units::step)
    {
        co_await step_one(
            timer, std::chrono::duration_cast<std::chrono::microseconds>(mp_units::to_chrono_duration(cruise_delay)));
    }

    // 5. Deceleration phase
    for (units::Steps i = ramp_steps; i >= 1 * units::step; i -= 1 * units::step)
    {
        const SecondsPerStep delay = 1 / mp_units::sqrt(2.0 * kAcceleration * i); // v = √(2a * s)
        co_await step_one(timer,
                          std::chrono::duration_cast<std::chrono::microseconds>(mp_units::to_chrono_duration(delay)));
    }
}

boost::asio::awaitable<void> Drv8825StepperMotorDriver::step_one(boost::asio::steady_timer &timer,
                                                                 std::chrono::microseconds wait_after)
{
    step_line_.set_value(step_offset_, gpiod::line::value::ACTIVE);
    timer.expires_after(kPulseWidth);
    co_await timer.async_wait(async::use_awaitable);
    step_line_.set_value(step_offset_, gpiod::line::value::INACTIVE);

    timer.expires_after(wait_after);
    co_await timer.async_wait(async::use_awaitable);
}

} // namespace cm
