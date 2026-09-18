module;
export module cm:execute_recipe;
import std;
import cm.core;
import mp_units;
import :ingredient;
import :recipe;

namespace cm {

export enum CommandStatus
{
    unknown,
    in_progress,
    finished
};

export class BasicCommandExecuter
{
  public:
    BasicCommandExecuter() = default;
    BasicCommandExecuter(BasicCommandExecuter&&) noexcept = delete;
    BasicCommandExecuter(const BasicCommandExecuter&) = delete;
    BasicCommandExecuter& operator=(const BasicCommandExecuter&) = delete;
    BasicCommandExecuter& operator=(BasicCommandExecuter&&) noexcept = delete;
    virtual ~BasicCommandExecuter() = default;
    virtual Task<void> execute_command(ManualCommand command) = 0;
    virtual Task<void> execute_command(DispenseCommand command) = 0;
    virtual void update_command_status(CommandId id, CommandStatus status) = 0;
};

export Task<void> execute_commands(Commands commands, std::shared_ptr<BasicCommandExecuter> command_executer);
} // namespace cm
