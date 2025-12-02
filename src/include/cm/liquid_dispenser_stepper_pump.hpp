// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <spdlog/fwd.h>
#include "hw/stepper_motor.hpp"
#include "liquid_dispenser.hpp"

namespace cm {

class StepperPumpLiquidDispenser : public LiquidDispenser
{
  public:
    StepperPumpLiquidDispenser(std::string identifier,
                               std::unique_ptr<StepperMotor> motor,
                               units::Litre source_volume,
                               units::StepsPerLitre steps_per_litre,
                               units::Litre tube_volume);
    async<void> dispense(units::Litre volume) override;
    /**
     * @brief
     * @attention Does not decrement the remaining_volume as it is only intended for calibration purposes.
     * @param steps
     * @return async<void>
     */
    async<void> dispense(units::Steps steps);
    void refill(units::Litre volume) override;
    units::Litre remaining_volume() const override;
    const std::string& name() const override;
    void update_steps_per_litre(units::StepsPerLitre steps_per_litre);

  private:
    std::string identifier_;
    std::unique_ptr<StepperMotor> motor_;
    units::StepsPerLitre steps_per_litre_{};
    units::Litre source_volume_{};
    units::Litre source_remaining_volume_{};
    units::Litre tube_volume_{1000 * mp_units::si::litre};
    bool tube_filled_{false};
    std::shared_ptr<spdlog::logger> logger_;
};
} // namespace cm
