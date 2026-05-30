module;
#include <boost/cobalt.hpp>
#include <boost/asio/steady_timer.hpp>

export module cm:execute_recipe;
import std;
import mp_units;
import :ingredient;
import :recipe;
import :units;
import :logging;

namespace cm {
namespace cobalt = boost::cobalt;

namespace {
template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};

cobalt::promise<void> process_manual_command(ManualCommand command)
{
    auto logger = log::create_or_get("recipe");
    cm::log::info{logger, "Process manual command {}", command.instruction};
    co_return;
}

cobalt::promise<void> process_dispense_command(DispenseCommand command)
{
    auto logger = log::create_or_get("recipe");

    boost::asio::steady_timer tim{co_await cobalt::this_coro::executor, std::chrono::milliseconds{500}};
    co_await tim.async_wait(cobalt::use_op);

    cm::log::info{logger, "Process dispense command {}", command.ingredient};
    co_return;
}

constexpr auto kProcessCommand = overloaded{[](const ManualCommand& command) { return process_manual_command(command); },
                                            [](const DispenseCommand& command) { return process_dispense_command(command); },
                                            [](std::monostate) -> cobalt::promise<void> { co_return; }};

} // namespace

export cobalt::promise<void> process_commands(Commands commands)
{
    for (auto&& c : commands) {
        co_await std::visit(overloaded{
                                [](const Command& command) { return std::visit(kProcessCommand, command); },
                                [](ParallelCommand commands) -> cobalt::promise<void> {
                                    std::vector<cobalt::promise<void>> parallel_group;
                                    parallel_group.reserve(commands.size());
                                    for (auto&& c : commands) {
                                        parallel_group.emplace_back(std::visit(kProcessCommand, c));
                                    }
                                    co_await cobalt::join(std::move(parallel_group));
                                    co_return;
                                },
                            },
                            c);
    }
}
} // namespace cm
