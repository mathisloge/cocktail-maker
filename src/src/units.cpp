#include <print>
#include <fmt/core.h>
#include <mp-units/systems/international.h>
#include <mp-units/systems/isq.h>
#include <mp-units/systems/si.h>

using namespace mp_units::si::unit_symbols;
using namespace mp_units::si::unit_symbols;

void test()
{
    mp_units::quantity<mp_units::si::milli<mp_units::non_si::litre>> x = 10 * mp_units::si::milli<mp_units::non_si::litre>;

    fmt::println("{}", x);
}
