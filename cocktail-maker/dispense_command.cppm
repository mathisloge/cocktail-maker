module;
#include <boost/cobalt.hpp>

export module cm:dispense_command;
import std;
import cm.core;
import :recipe;
import :dispenser;

namespace cobalt = boost::cobalt;

namespace cm {
cobalt::promise<void> execute_command(DispenseCommand cmd, std::shared_ptr<Dispenser> dispenser)
{
    co_await dispenser->dispense(cmd.volume);
}

} // namespace cm
