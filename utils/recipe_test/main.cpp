#include <thread>
#include <boost/asio.hpp>
#include <cm/hw/drv8825_stepper_moter.hpp>
#include <fmt/core.h>

using namespace mp_units::si::unit_symbols;

struct Ingredient
{
    std::uint32_t id;
    std::string name;
};

struct LiquidProductionStep
{
    std::uint32_t ingredient_id;
    mp_units::quantity_point<mp_units::si::litre> amount;
};

int main()
{

    return 0;
}
