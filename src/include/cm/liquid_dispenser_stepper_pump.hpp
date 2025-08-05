#pragma once
#include "hw/stepper_motor.hpp"
#include "liquid_dispenser.hpp"
namespace cm
{

class StepperPumpLiquidDispenser : public LiquidDispenser
{
  public:
    StepperPumpLiquidDispenser(std::string identifier,
                               std::unique_ptr<StepperMotor> motor,
                               units::Litre source_volume,
                               units::StepsPerLitre calibration,
                               units::Litre tube_volume);
    boost::asio::awaitable<void> dispense(units::Litre volume) override;
    void refill(units::Litre volume) override;
    units::Litre remaining_volume() const override;
    const std::string &name() const override;

  private:
    std::string identifier_;
    std::unique_ptr<StepperMotor> motor_;
    units::StepsPerLitre steps_per_litre_{};
    units::Litre source_volume_{};
    units::Litre source_remaining_volume_{};
    units::Litre tube_volume_{1000 * mp_units::si::litre};
    bool tube_filled_{false};
};
} // namespace cm
