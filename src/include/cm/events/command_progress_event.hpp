#pragma once
#include "cm/commands/command_id.hpp"
#include "cm/units.hpp"
namespace cm
{
struct CommandProgress
{
    CommandId cmd_id;
    units::quantity<units::percent> progress;
};
} // namespace cm
