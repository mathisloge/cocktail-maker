module;
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt.hpp>

export module cm:execute_recipe;
import std;
import cm.core;
import mp_units;
import :ingredient;
import :recipe;

namespace cm {
namespace cobalt = boost::cobalt;

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
    virtual cobalt::promise<void> execute_command(ManualCommand command) = 0;
    virtual cobalt::promise<void> execute_command(DispenseCommand command) = 0;
    virtual void update_command_status(CommandId id, CommandStatus status) = 0;
};

export cobalt::promise<void> execute_commands(Commands commands, std::shared_ptr<BasicCommandExecuter> command_executer)
{

    const auto process_command = detail::Overloaded{[command_executer](ManualCommand command) -> cobalt::promise<void> {
                                                        const auto id = command.id;
                                                        command_executer->update_command_status(id, CommandStatus::in_progress);
                                                        co_await command_executer->execute_command(std::move(command));
                                                        command_executer->update_command_status(id, CommandStatus::finished);
                                                    },
                                                    [command_executer](DispenseCommand command) -> cobalt::promise<void> {
                                                        const auto id = command.id;
                                                        command_executer->update_command_status(id, CommandStatus::in_progress);
                                                        co_await command_executer->execute_command(std::move(command));
                                                        command_executer->update_command_status(id, CommandStatus::finished);
                                                    },
                                                    [](std::monostate) -> cobalt::promise<void> { co_return; }};

    for (auto&& c : commands) {
        co_await std::visit(detail::Overloaded{
                                [process_command](const Command& command) { return std::visit(process_command, command); },
                                [process_command](ParallelCommand commands) -> cobalt::promise<void> {
                                    std::vector<cobalt::promise<void>> parallel_group;
                                    parallel_group.reserve(commands.size());
                                    for (auto&& c : commands) {
                                        parallel_group.emplace_back(std::visit(process_command, c));
                                    }
                                    co_await cobalt::join(std::move(parallel_group));
                                    co_return;
                                },
                            },
                            c);
    }
}
} // namespace cm
