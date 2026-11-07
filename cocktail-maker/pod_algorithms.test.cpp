#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <stdexec/execution.hpp>
import std;
import cm.core;
import cm.testing;
import cm;

namespace units = cm::units;

namespace {
/// A pod whose EmergencyStop waits until the test lets it finish.
class SlowStopPod final : public cm::IPod
{
  public:
    explicit SlowStopPod(cm::AwaitableBool& may_finish)
        : may_finish_{may_finish}
    {
    }

    cm::PodId pod_id() const override
    {
        return cm::PodId{"Slow"};
    }

    cm::Task<void> run(std::unique_ptr<cm::PodState> /*state*/) override
    {
        co_return;
    }

    std::expected<std::unique_ptr<cm::Dispenser>, cm::DispenserNotFoundError> create_dispenser(cm::DispenserId id) override
    {
        return std::unexpected{cm::DispenserNotFoundError{pod_id(), id}};
    }

    cm::Task<void> highlight_dispenser(cm::DispenserId /*id*/, std::chrono::milliseconds /*duration*/) override
    {
        co_return;
    }

    cm::Task<void> load_cell_calibrate_with_ref_weight(cm::DispenserId /*id*/, units::Grams /*grams*/) override
    {
        co_return;
    }

    cm::Task<void> load_cell_tare(cm::DispenserId /*id*/) override
    {
        co_return;
    }

    cm::Task<units::Grams> load_cell_weight(cm::DispenserId /*id*/) override
    {
        co_return units::Grams{};
    }

    cm::Task<cm::DispenseReport> dispense(cm::DispenserId /*id*/, units::Litre /*volume*/) override
    {
        co_return cm::DispenseReport{};
    }

    cm::Task<units::Litre> dispenser_calibrate(cm::DispenserId /*id*/, std::chrono::milliseconds /*run_time*/) override
    {
        co_return units::Litre{};
    }

    cm::Task<void> force_safe_state() override
    {
        co_await may_finish_.async_wait();
        ++finished;
    }

    int finished = 0;

  private:
    cm::AwaitableBool& may_finish_;
};
} // namespace

TEST_CASE("force_safe_state_all keeps a pod alive until its stop has finished", "[pod_algorithms]")
{
    boost::asio::io_context ctx;
    cm::AwaitableBool may_finish{ctx.get_executor()};
    auto pod = std::make_shared<SlowStopPod>(may_finish);
    const std::weak_ptr<SlowStopPod> weak = pod;

    // A locking view hands out shared pointers that only live for one step of the loop, as PodRegistry's does.
    const std::vector<std::weak_ptr<cm::IPod>> registered{pod};
    auto locked = registered | std::views::transform([](const std::weak_ptr<cm::IPod>& p) { return p.lock(); });

    bool alive_during_stop = false;
    const auto scenario = [&]() -> cm::Task<void> {
        const auto release = [&]() -> cm::Task<void> {
            pod.reset();
            alive_during_stop = not weak.expired();
            may_finish = true;
            co_return;
        };
        std::vector<cm::Task<void>> tasks;
        tasks.emplace_back(cm::force_safe_state_all(locked));
        tasks.emplace_back(release());
        co_await cm::join_all(std::move(tasks));
    };

    REQUIRE(cm::run_until_complete(ctx, scenario()));
    CHECK(alive_during_stop);
    CHECK(weak.expired());
}
