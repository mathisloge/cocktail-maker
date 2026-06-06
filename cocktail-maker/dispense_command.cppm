module;
#include <boost/cobalt.hpp>

export module cm:dispense_command;
import std;
import :recipe;
import :logging;
import :async_machine_interface;

namespace cobalt = boost::cobalt;

namespace cm {
cobalt::promise<void> execute_command(DispenseCommand cmd, std::shared_ptr<BasicAsyncMachineInterface> machine_interface)
{
    const auto pump = machine_interface->dispenser_for_ingredient(cmd.ingredient);
    co_await pump->dispense(cmd.volume);
}

} // namespace cm
