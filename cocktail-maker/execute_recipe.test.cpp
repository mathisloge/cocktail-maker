#include <boost/cobalt.hpp>
#include <catch2/catch_test_macros.hpp>
import cm;

using namespace cm;
namespace cobalt = boost::cobalt;

namespace {
class MachineAdapterMock : public BasicCommandExecuter
{
  public:
    cobalt::task<void> execute_command(ManualCommand command) override
    {
        co_return;
    }

    cobalt::task<void> execute_command(DispenseCommand command) override
    {
        co_return;
    }
};

} // namespace

TEST_CASE("Test execute_commands", "[execute_commands]")
{
}
