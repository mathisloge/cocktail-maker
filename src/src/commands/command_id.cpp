#include "cm/commands/command_id.hpp"
namespace cm
{
CommandId generate_unique_command_id()
{
    static CommandId id{0};
    return ++id;
}
} // namespace cm
