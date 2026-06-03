module;
#include <boost/asio.hpp>
#include <boost/cobalt.hpp>

export module cm:async_machine_interface;

import std;
import :logging;
import :units;

namespace cm {

namespace cobalt = boost::cobalt;
namespace asio = boost::asio;

template <typename AsyncStream>
class AsyncMachineInterface
{
  public:
    cobalt::promise<void> aquire_system_info();
    cobalt::promise<void> emergency_stop();

    cobalt::promise<void> open_valve(int valve_id);
    cobalt::promise<void> close_valve(int valve_id);

    cobalt::promise<void> motor_step(int steps);
    cobalt::promise<void> calibrate_pump(int steps_per_litre);
    cobalt::promise<void> pump(units::Litre volume, int pump_id);
    cobalt::promise<void> wait_for_finish(int pump_id);

    cobalt::promise<void> load_cell_calibrate_offset(int load_cell_id);
    cobalt::promise<void> load_cell_calibrate_ref_weight(int grams);
    cobalt::promise<int> load_cell_measure_weight();
    cobalt::promise<void> load_cell_set_empty(int grams);
};

} // namespace cm
