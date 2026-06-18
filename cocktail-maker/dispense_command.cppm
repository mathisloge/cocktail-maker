module;
#include <boost/cobalt.hpp>

export module cm:dispense_command;
import std;
import :recipe;
import :logging;
import :dispenser;

namespace cobalt = boost::cobalt;

namespace cm {
cobalt::promise<void> execute_command(DispenseCommand cmd, std::shared_ptr<Dispenser> dispenser)
{
    co_await dispenser->dispense(cmd.volume);
}

} // namespace cm
