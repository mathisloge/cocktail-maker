#pragma once

/// A dispenser for tests. Include it after `import cm;`.
class FakeDispenser final : public cm::Dispenser
{
  public:
    struct Refused
    {
    };

    using Step = std::variant<cm::DispenseReport, std::exception_ptr>;

    cm::Task<cm::DispenseReport> dispense(cm::units::Litre volume) override
    {
        requested.push_back(volume);
        if (steps.empty()) {
            throw std::logic_error{"FakeDispenser got a pour nobody expected"};
        }
        auto step = std::move(steps.front());
        steps.pop_front();
        if (auto* error = std::get_if<std::exception_ptr>(&step)) {
            std::rethrow_exception(*error);
        }
        const auto report = std::get<cm::DispenseReport>(step);
        reading -= report.amount.numerical_value_in(cm::units::milli_litre) * cm::units::si::gram;
        co_return report;
    }

    cm::Task<cm::units::Litre> calibrate(std::chrono::milliseconds /*run_time*/) override
    {
        co_return 0.0 * cm::units::milli_litre;
    }

    cm::Task<cm::units::Grams> load_cell_weight() override
    {
        ++weight_reads;
        if (pod_gone) {
            throw std::runtime_error{"Pod lifetime exceeded"};
        }
        if (no_load_cell) {
            throw cm::PodReceiveError{cm::PodId{"Fake"}, proto::field::ErrorCodeVal::NoLoadCell};
        }
        co_return reading;
    }

    cm::Task<void> load_cell_calibrate_with_ref_weight(cm::units::Grams /*grams*/) override
    {
        co_return;
    }

    cm::Task<void> load_cell_tare() override
    {
        co_return;
    }

    cm::Task<void> highlight(std::chrono::milliseconds /*duration*/) override
    {
        co_return;
    }

    cm::units::Grams reading = 0.0 * cm::units::si::gram;
    std::deque<Step> steps;
    std::vector<cm::units::Litre> requested;
    int weight_reads = 0;
    bool no_load_cell = false;
    bool pod_gone = false;
};
