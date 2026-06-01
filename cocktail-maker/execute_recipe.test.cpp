#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt.hpp>
#include <catch2/catch_test_macros.hpp>
import cm;

using namespace cm;
namespace cobalt = boost::cobalt;

namespace {
class MachineAdapterMock : public BasicMachineAdapter
{
  public:
    cobalt::promise<void> execute_command(ManualCommand command) override
    {
        co_return;
    }

    cobalt::promise<void> execute_command(DispenseCommand command) override
    {
        co_return;
    }
};

} // namespace

TEST_CASE("Test process_commands", "[process_commands]")
{
}
