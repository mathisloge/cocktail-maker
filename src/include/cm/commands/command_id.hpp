#pragma once
#include <cstdint>
namespace cm
{
using CommandId = std::uint32_t;

CommandId generate_unique_command_id();
} // namespace cm
