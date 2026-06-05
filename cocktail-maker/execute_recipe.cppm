module;
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt.hpp>

export module cm:execute_recipe;
import std;
import mp_units;
import :ingredient;
import :recipe;
import :units;
import :logging;
import :overloaded;

namespace cm {
namespace cobalt = boost::cobalt;

export enum CommandStatus
{
    unknown,
    in_progress,
    finished
};

export class BasicMachineAdapter
{
  public:
    BasicMachineAdapter() = default;
    BasicMachineAdapter(BasicMachineAdapter&&) noexcept = delete;
    BasicMachineAdapter(const BasicMachineAdapter&) = delete;
    BasicMachineAdapter& operator=(const BasicMachineAdapter&) = delete;
    BasicMachineAdapter& operator=(BasicMachineAdapter&&) noexcept = delete;
    virtual ~BasicMachineAdapter() = default;
    virtual cobalt::promise<void> execute_command(ManualCommand command) = 0;
    virtual cobalt::promise<void> execute_command(DispenseCommand command) = 0;
    virtual void update_command_status(CommandId id, CommandStatus status) = 0;
};

export cobalt::promise<void> process_commands(Commands commands, std::shared_ptr<BasicMachineAdapter> machine_adapter)
{

    const auto process_command = detail::Overloaded{[machine_adapter](ManualCommand command) -> cobalt::promise<void> {
                                                        const auto id = command.id;
                                                        machine_adapter->update_command_status(id, CommandStatus::in_progress);
                                                        co_await machine_adapter->execute_command(std::move(command));
                                                        machine_adapter->update_command_status(id, CommandStatus::finished);
                                                    },
                                                    [machine_adapter](DispenseCommand command) -> cobalt::promise<void> {
                                                        const auto id = command.id;
                                                        machine_adapter->update_command_status(id, CommandStatus::in_progress);
                                                        co_await machine_adapter->execute_command(std::move(command));
                                                        machine_adapter->update_command_status(id, CommandStatus::finished);
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
