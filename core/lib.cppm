module;
#include <mp-units/core.h>
#include <mp-units/systems/si/units.h>
// WORKAROUND: Include boost.asio in the primary module's Global Module Fragment
// to prevent GCC from emitting duplicate anonymous namespace symbols when 
// importing the partitions below.
#include <boost/asio.hpp> 


export module cm.core;
export import :asio;
export import :logging;
export import :units;
export import :comms_adapter;
export import :awaitable_bool;
export import :overloaded;
export import :strong_type;
export import :any_io_stream;
