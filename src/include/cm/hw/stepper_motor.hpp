#pragma once
#include "motor.hpp"
#include <mp-units/core.h>
#include <mp-units/systems/si/units.h>

namespace cm
{

inline constexpr struct Step final : mp_units::named_unit<"steps", mp_units::one>
{
} step;

using Steps = mp_units::quantity<step, std::int32_t>;

class StepperMotor : public Motor
{};
} // namespace cm
