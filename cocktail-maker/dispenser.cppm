module;
#include <boost/cobalt.hpp>

export module cm:dispenser;
import std;
import cm.core;

namespace cobalt = boost::cobalt;

namespace cm {

export class DispenserNotFoundError : public std::runtime_error
{
    using runtime_error::runtime_error;
};

export using DispenserId = strong_type<int, struct DispenserIdTag, Comparable, Hashable, Formattable>;

export class Dispenser
{
  public:
    Dispenser() = default;
    virtual ~Dispenser() = default;
    Dispenser(const Dispenser&) = delete;
    Dispenser(Dispenser&&) noexcept = delete;
    Dispenser& operator=(const Dispenser&) = delete;
    Dispenser& operator=(Dispenser&&) noexcept = delete;

    virtual cobalt::promise<void> dispense(units::Litre volume) = 0;
};

} // namespace cm
