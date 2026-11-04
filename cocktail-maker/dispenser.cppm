module;
export module cm:dispenser;
import std;
import cm.core;
import :pod_types;

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

/// Why a pour ended.
export enum class DispenseOutcome
{
    completed,
    bottle_empty,
    timed_out,
};

/// What a pour really delivered, measured at rest, and why it ended.
export struct DispenseReport
{
    units::Litre amount;
    DispenseOutcome outcome;
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

    virtual Task<DispenseReport> dispense(units::Litre volume) = 0;
    virtual Task<units::Litre> calibrate(std::chrono::milliseconds run_time) = 0;
    /// Reads the load cell. The weight is relative to its zero point, so it can be negative.
    virtual Task<units::Grams> load_cell_weight() = 0;
    virtual Task<void> load_cell_calibrate_with_ref_weight(units::Grams grams) = 0;
    virtual Task<void> load_cell_tare() = 0;
    virtual Task<void> highlight(std::chrono::milliseconds duration) = 0;
};

} // namespace cm
