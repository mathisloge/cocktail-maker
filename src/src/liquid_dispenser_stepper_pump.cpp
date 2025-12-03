// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/liquid_dispenser_stepper_pump.hpp"
#include <cm/logging.hpp>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

namespace cm {
constexpr units::StepsPerSecond kVelocity{800 * units::step / mp_units::si::second};

StepperPumpLiquidDispenser::StepperPumpLiquidDispenser(std::string identifier,
                                                       std::unique_ptr<StepperMotor> motor,
                                                       units::Litre source_volume,
                                                       units::StepsPerLitre steps_per_litre,
                                                       units::Litre tube_volume)
    : identifier_{std::move(identifier)}
    , motor_{std::move(motor)}
    , source_volume_{source_volume}
    , source_remaining_volume_{source_volume_}
    , steps_per_litre_{steps_per_litre}
    , tube_volume_{tube_volume}
    , logger_{LoggingContext::instance().create_logger(identifier_)}
{
    SPDLOG_LOGGER_INFO(logger_,
                       "StepperPumpLiquidDispenser \"{}\" initialized with {}, {} source volume and {} "
                       "tube volume",
                       identifier_,
                       steps_per_litre_,
                       source_volume_,
                       tube_volume_);
}

void StepperPumpLiquidDispenser::refill(units::Litre volume)
{
    SPDLOG_LOGGER_DEBUG(logger_, "Refilled {}", volume);
    source_remaining_volume_ = volume;
    tube_filled_ = false;
}

async<void> StepperPumpLiquidDispenser::dispense(mp_units::quantity<mp_units::si::litre> volume)
{
    SPDLOG_LOGGER_DEBUG(logger_, "Going to dispense {}.", volume);
    co_await motor_->enable();

    if (not tube_filled_) {
        const auto fill_steps = mp_units::value_cast<std::int32_t>(tube_volume_ * steps_per_litre_);
        co_await motor_->step(fill_steps, kVelocity);
        // tube_filled_ = true; DISABLE for now, as the liquid always flows back into the container
    }

    source_remaining_volume_ -= volume;
    const auto steps = mp_units::value_cast<std::int32_t>(volume * steps_per_litre_);
    co_await motor_->step(steps, kVelocity);

    co_await motor_->disable();
    SPDLOG_LOGGER_DEBUG(logger_, "Finished dispensing {}.", volume);
}

async<void> StepperPumpLiquidDispenser::dispense(units::Steps steps)
{
    co_await motor_->enable();
    co_await motor_->step(steps, kVelocity);
    co_await motor_->disable();
}

mp_units::quantity<mp_units::si::litre> StepperPumpLiquidDispenser::remaining_volume() const
{
    return source_remaining_volume_;
}

units::Litre StepperPumpLiquidDispenser::volume() const
{
    return source_volume_;
}

const std::string& StepperPumpLiquidDispenser::name() const
{
    return identifier_;
}

void StepperPumpLiquidDispenser::update_steps_per_litre(units::StepsPerLitre steps_per_litre)
{
    SPDLOG_LOGGER_DEBUG(logger_, "Updated calibration value to '{}' from '{}'", steps_per_litre, steps_per_litre_);
    steps_per_litre_ = steps_per_litre;
}

} // namespace cm
