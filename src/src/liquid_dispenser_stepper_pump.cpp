#include "cm/liquid_dispenser_stepper_pump.hpp"
#include <fmt/core.h>

namespace cm
{
StepperPumpLiquidDispenser::StepperPumpLiquidDispenser(std::string identifier,
                                                       std::unique_ptr<StepperMotor> motor,
                                                       units::Litre source_volume,
                                                       units::StepsPerLitre calibration,
                                                       units::Litre tube_volume)
    : identifier_{std::move(identifier)}
    , motor_{std::move(motor)}
    , source_volume_{source_volume}
    , source_remaining_volume_{source_volume_}
    , steps_per_litre_{calibration}
    , tube_volume_{tube_volume}
{
    fmt::println("StepperPumpLiquidDispenser \"{}\" initialized with {}, {} source volume and {} tube volume",
                 identifier,
                 steps_per_litre_,
                 source_volume,
                 tube_volume_);
}

boost::asio::awaitable<void> StepperPumpLiquidDispenser::dispense(mp_units::quantity<mp_units::si::litre> volume)
{
    constexpr units::StepsPerSecond kVelocity{1000 * units::step / mp_units::si::second};
    co_await motor_->enable();

    if (not tube_filled_)
    {
        const auto fill_steps = mp_units::value_cast<std::int32_t>(tube_volume_ * steps_per_litre_);
        co_await motor_->step(fill_steps, kVelocity);
        tube_filled_ = true;
    }

    source_remaining_volume_ -= volume;
    const auto steps = mp_units::value_cast<std::int32_t>(volume * steps_per_litre_);
    co_await motor_->step(steps, kVelocity);

    co_await motor_->disable();
}

mp_units::quantity<mp_units::si::litre> StepperPumpLiquidDispenser::remaining_volume() const
{
    return source_remaining_volume_;
}

const std::string &StepperPumpLiquidDispenser::name() const
{
    return identifier_;
}

} // namespace cm
