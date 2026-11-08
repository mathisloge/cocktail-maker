#pragma once

// A fake Pod for tests that need one in the registry. Include it after the imports, like slint_events.test.hpp. The
// including unit also needs <proto/field/ErrorCodeCommon.h> in front of its imports.

/// A pod that refuses every pour and counts how often it was stopped.
class RefusingPod final : public cm::IPod, public std::enable_shared_from_this<RefusingPod>
{
  public:
    cm::PodId pod_id() const override
    {
        return cm::PodId{"Refusing"};
    }

    cm::Task<void> run(std::unique_ptr<cm::PodState> /*state*/) override
    {
        co_return;
    }

    std::expected<std::unique_ptr<cm::Dispenser>, cm::DispenserNotFoundError> create_dispenser(cm::DispenserId id) override
    {
        return std::make_unique<cm::Pump>(weak_from_this(), pod_id(), id);
    }

    cm::Task<void> highlight_dispenser(cm::DispenserId /*id*/, std::chrono::milliseconds /*duration*/) override
    {
        co_return;
    }

    cm::Task<void> load_cell_calibrate_with_ref_weight(cm::DispenserId /*id*/, cm::units::Grams /*grams*/) override
    {
        co_return;
    }

    cm::Task<void> load_cell_tare(cm::DispenserId /*id*/) override
    {
        co_return;
    }

    cm::Task<cm::units::Grams> load_cell_weight(cm::DispenserId /*id*/) override
    {
        co_return 0.0 * cm::units::si::gram;
    }

    cm::Task<cm::DispenseReport> dispense(cm::DispenserId /*id*/, cm::units::Litre /*volume*/) override
    {
        throw cm::PodReceiveError{pod_id(), proto::field::ErrorCodeVal::HardwareFault};
        co_return cm::DispenseReport{};
    }

    cm::Task<cm::units::Litre> dispenser_calibrate(cm::DispenserId /*id*/, std::chrono::milliseconds /*run_time*/) override
    {
        co_return 0.0 * cm::units::milli_litre;
    }

    cm::Task<void> force_safe_state() override
    {
        ++stops;
        if (stop_times_out) {
            throw cm::PodTimeoutError{pod_id()};
        }
        co_return;
    }

    int stops = 0;
    bool stop_times_out = false;
};
