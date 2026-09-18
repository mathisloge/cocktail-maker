module;
#include <stdexec/execution.hpp>

module cm:execute_recipe_impl;
import std;
import cm.core;
import :recipe;
import :execute_recipe;

namespace cm {

Task<void> execute_commands(Commands commands, std::shared_ptr<BasicCommandExecuter> command_executer)
{

    const auto process_command = detail::Overloaded{[command_executer](ManualCommand command) -> Task<void> {
                                                        const auto id = command.id;
                                                        command_executer->update_command_status(id, CommandStatus::in_progress);
                                                        co_await command_executer->execute_command(std::move(command));
                                                        command_executer->update_command_status(id, CommandStatus::finished);
                                                    },
                                                    [command_executer](DispenseCommand command) -> Task<void> {
                                                        const auto id = command.id;
                                                        command_executer->update_command_status(id, CommandStatus::in_progress);
                                                        co_await command_executer->execute_command(std::move(command));
                                                        command_executer->update_command_status(id, CommandStatus::finished);
                                                    },
                                                    [](std::monostate) -> Task<void> { co_return; }};

    for (auto&& c : commands) {
        co_await std::visit(detail::Overloaded{
                                [process_command](const Command& command) { return std::visit(process_command, command); },
                                [process_command](ParallelCommand commands) -> Task<void> {
                                    std::vector<Task<void>> parallel_group;
                                    parallel_group.reserve(commands.size());
                                    for (auto&& c : commands) {
                                        parallel_group.emplace_back(std::visit(process_command, c));
                                    }
                                    co_await join_all(std::move(parallel_group));
                                },
                            },
                            c);
    }
}
} // namespace cm
