#pragma once
#include <memory>
#include "hw/motor.hpp"
namespace cm
{
class StepperPump : public LiquidDispenser
{
  public:
    Pump(std::unique_ptr<Motor> motor);

  private:
    std::unique_ptr<Motor> motor_;
};
} // namespace cm
