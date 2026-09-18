#include <catch2/catch_test_macros.hpp>
#include <stdexec/execution.hpp>
import cm.core;
import cm;

using namespace cm;

namespace {
class MachineAdapterMock : public BasicCommandExecuter
{
  public:
    Task<void> execute_command(ManualCommand command) override
    {
        co_return;
    }

    Task<void> execute_command(DispenseCommand command) override
    {
        co_return;
    }
};

} // namespace

TEST_CASE("Test execute_commands", "[execute_commands]")
{
}
