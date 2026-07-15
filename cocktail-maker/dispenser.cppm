module;
#include <boost/cobalt/promise.hpp>

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

    virtual cobalt::promise<units::Litre> dispense(units::Litre volume) = 0;
    virtual cobalt::promise<void> load_cell_calibrate_with_ref_weight(units::Grams grams) = 0;
    virtual cobalt::promise<void> load_cell_tare() = 0;
    virtual cobalt::promise<void> highlight(std::chrono::milliseconds duration) = 0;
};

} // namespace cm
