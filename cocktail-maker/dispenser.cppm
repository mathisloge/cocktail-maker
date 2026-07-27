module;
#include <boost/cobalt/promise.hpp>

export module cm:dispenser;
import std;
import cm.core;
import :pod_types;

namespace cobalt = boost::cobalt;

namespace cm {

export using DispenserId = strong_type<int, struct DispenserIdTag, Comparable, Hashable, Formattable>;

export class DispenserNotFoundError : public std::runtime_error
{
  public:
    DispenserNotFoundError(PodId pod_id, DispenserId dispenser_id)
        : runtime_error{std::format("Dispenser '{}' not found on pod '{}'", dispenser_id, pod_id)}
        , pod_id_{pod_id}
        , dispenser_id_{dispenser_id}
    {
    }

    PodId pod_id() const
    {
        return pod_id_;
    }

    DispenserId dispenser_id() const
    {
        return dispenser_id_;
    }

  private:
    PodId pod_id_;
    DispenserId dispenser_id_;
};

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
