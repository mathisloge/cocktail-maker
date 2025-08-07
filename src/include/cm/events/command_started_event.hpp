#pragma once
#include "cm/commands/command_id.hpp"
namespace cm
{
struct CommandStarted
{
    CommandId cmd_id;
};
} // namespace cm
