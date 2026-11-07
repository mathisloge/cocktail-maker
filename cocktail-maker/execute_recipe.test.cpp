#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexec/execution.hpp>
import std;
import mp_units;
import cm.core;
import cm.testing;
import cm;

using namespace cm;
using namespace std::chrono_literals;

namespace {
using Update = std::pair<CommandId, CommandStatus>;

/// Takes a while for every command and notes each status it is told.
class RecordingExecuter final : public BasicCommandExecuter
{
  public:
    explicit RecordingExecuter(boost::asio::any_io_executor executor)
        : executor_{std::move(executor)}
    {
    }

    Task<void> execute_command(ManualCommand /*command*/) override
    {
        co_await async_sleep(executor_, 10ms);
    }

    Task<void> execute_command(DispenseCommand /*command*/) override
    {
        co_await async_sleep(executor_, 10ms);
    }

    void update_command_status(CommandId id, CommandStatus status) override
    {
        updates.emplace_back(id, status);
    }

    std::vector<Update> updates;

  private:
    boost::asio::any_io_executor executor_;
};

DispenseCommand rum(int id)
{
    return DispenseCommand{.id = CommandId{id}, .ingredient = IngredientId{"rum"}, .volume = 40 * units::milli_litre};
}
} // namespace

TEST_CASE("execute_commands runs the steps one after the other", "[execute_commands]")
{
    boost::asio::io_context ioc;
    auto executer = std::make_shared<RecordingExecuter>(ioc.get_executor());
    const Commands commands{
        Command{ManualCommand{.id = CommandId{1}, .instruction = "Eis"}},
        Command{rum(2)},
    };

    REQUIRE(run_until_complete(ioc, execute_commands(commands, executer)));

    CHECK(executer->updates == std::vector<Update>{
                                   {CommandId{1}, CommandStatus::in_progress},
                                   {CommandId{1}, CommandStatus::finished},
                                   {CommandId{2}, CommandStatus::in_progress},
                                   {CommandId{2}, CommandStatus::finished},
                               });
}

TEST_CASE("execute_commands starts every step of a parallel group before one finishes", "[execute_commands]")
{
    boost::asio::io_context ioc;
    auto executer = std::make_shared<RecordingExecuter>(ioc.get_executor());
    const Commands commands{ParallelCommand{Command{rum(1)}, Command{rum(2)}}};

    REQUIRE(run_until_complete(ioc, execute_commands(commands, executer)));

    REQUIRE(executer->updates.size() == 4);
    CHECK(executer->updates[0].second == CommandStatus::in_progress);
    CHECK(executer->updates[1].second == CommandStatus::in_progress);
}
