#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <exec/asio/use_sender.hpp>
#include <exec/when_any.hpp>
#include <proto/field/ErrorCodeCommon.h>
#include <stdexec/execution.hpp>

import std;
import mp_units;
import cm.core;
import cm.testing;
import cm;
import cm.sim;

using namespace std::chrono_literals;

namespace {
using Socket = cm::sim::Socket;
namespace units = cm::units;
using ErrorCode = proto::field::ErrorCodeVal;

const cm::DispenserId kDispenser{0};
const cm::DispenserId kOtherDispenser{1};
const cm::DispenserId kWithoutLoadCell{4};

class TestPodState final : public cm::PodState
{
  public:
    TestPodState(cm::AwaitableBool& connected, cm::AwaitableBool& incompatible)
        : connected_{connected}
        , incompatible_{incompatible}
    {
    }

    void update_info(cm::PodInfo /*info*/) override
    {
    }

    void update_state(ConnectionState state) override
    {
        if (state == ConnectionState::connected) {
            connected_ = true;
        }
        if (state == ConnectionState::incompatible) {
            incompatible_ = true;
        }
    }

  private:
    cm::AwaitableBool& connected_;
    cm::AwaitableBool& incompatible_;
};

/// Reports the connection, but keeps nothing of what it is told.
class ForgetfulPodState final : public cm::PodState
{
  public:
    explicit ForgetfulPodState(cm::AwaitableBool& connected)
        : connected_{connected}
    {
    }

    void update_info(cm::PodInfo /*info*/) override
    {
    }

    void update_state(ConnectionState state) override
    {
        if (state == ConnectionState::connected) {
            connected_ = true;
        }
    }

  private:
    cm::AwaitableBool& connected_;
};

/// A real `cm::Pod` that talks to a simulated client over a socket pair.
struct SimPodFixture
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    cm::AwaitableBool is_connected{ctx.get_executor()};
    cm::AwaitableBool is_incompatible{ctx.get_executor()};
    std::shared_ptr<cm::sim::Client<Socket>> client =
        std::make_shared<cm::sim::Client<Socket>>(scope, "SimPod", cm::Version{.major = 2});
    std::shared_ptr<cm::Pod> pod = make_pod();

    std::shared_ptr<cm::Pod> make_pod()
    {
        return std::make_shared<cm::Pod>(cm::sim::open_station_stream(*client));
    }

    /// Replaces the client and the pod, so it has to be called before `run_test`.
    void configure(cm::sim::ClientConfig config)
    {
        client = std::make_shared<cm::sim::Client<Socket>>(scope, "SimPod", cm::Version{.major = 2}, std::move(config));
        pod = make_pod();
    }

    auto connected()
    {
        return is_connected.async_wait();
    }

    /// Runs `test_coro` while the pod and the client are running, and rethrows what it failed with.
    template <typename CoroutineFunc>
    void run_test(CoroutineFunc&& test_coro)
    {
        run_test_with(std::make_unique<TestPodState>(is_connected, is_incompatible), std::forward<CoroutineFunc>(test_coro));
    }

    /// Like `run_test`, with `state` observing the pod.
    template <typename CoroutineFunc>
    void run_test_with(std::unique_ptr<cm::PodState> state, CoroutineFunc&& test_coro)
    {
        std::exception_ptr error;
        auto test_wrapper = [&]() -> cm::Task<void> {
            try {
                co_await exec::when_any(test_coro(), pod->run(std::move(state)));
            }
            catch (...) {
                error = std::current_exception();
            }
        };

        scope.spawn(client->run());
        cm::run_until_complete(ctx, test_wrapper());

        cm::run_until_complete(ctx, scope.stop_and_join());

        if (error) {
            std::rethrow_exception(error);
        }
    }
};
} // namespace

TEST_CASE_METHOD(SimPodFixture, "A Pod knows its id without its state observer", "[sim][pod]")
{
    run_test_with(std::make_unique<ForgetfulPodState>(is_connected), [this]() -> cm::Task<void> {
        co_await connected();
        CHECK(pod->pod_id() == cm::PodId{"SimPod"});
        CHECK(pod->create_dispenser(kDispenser).has_value());
    });
}

TEST_CASE_METHOD(SimPodFixture, "Tare and reference calibration succeed at their real length", "[sim][pod]")
{
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        co_await pod->load_cell_tare(kDispenser);
        co_await pod->load_cell_calibrate_with_ref_weight(kDispenser, 100 * units::si::gram);
    });
}

TEST_CASE_METHOD(SimPodFixture, "A calibration run returns the measured amount", "[sim][calibration]")
{
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const units::Litre amount = co_await pod->dispenser_calibrate(kDispenser, 2s);
        CHECK(amount > 0 * units::milli_litre);
    });
}

TEST_CASE_METHOD(SimPodFixture, "Replies within the Station's timeouts are accepted", "[sim][pod]")
{
    auto config = cm::sim::fast_test_config();
    config.ack_delay = 300ms;
    config.quick_reply = 300ms;
    configure(config);
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        std::ignore = co_await pod->dispense(kDispenser, 5 * units::milli_litre);
        std::ignore = co_await pod->dispenser_calibrate(kDispenser, 100ms);
        std::ignore = co_await pod->load_cell_weight(kDispenser);
        co_await pod->force_safe_state();
    });
}

TEST_CASE_METHOD(SimPodFixture, "The weight comes back relative to the zero point", "[sim][pod]")
{
    configure(cm::sim::fast_test_config());
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        co_await pod->load_cell_tare(kDispenser);
        std::ignore = co_await pod->dispense(kDispenser, 100 * units::milli_litre);
        const units::Grams weight = co_await pod->load_cell_weight(kDispenser);
        CHECK(weight == -100 * units::si::gram);
    });
}

TEST_CASE_METHOD(SimPodFixture, "A refused weight read is a Nak and not a protocol error", "[sim][pod]")
{
    configure(cm::sim::fast_test_config());
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        try {
            std::ignore = co_await pod->load_cell_weight(kWithoutLoadCell);
            FAIL("Expected a PodReceiveError");
        }
        catch (const cm::PodReceiveError& error) {
            CHECK(error.error_code() == ErrorCode::NoLoadCell);
        }
    });
}

TEST_CASE_METHOD(SimPodFixture, "A volume is rounded to the nearest millilitre", "[sim][pod]")
{
    configure(cm::sim::fast_test_config());
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        std::ignore = co_await pod->dispense(kDispenser, 19.999 * units::milli_litre);
        CHECK(client->stats().last_dispense_ml == 20);
    });
}

TEST_CASE_METHOD(SimPodFixture, "An amount that rounds to nothing isn't sent", "[sim][pod]")
{
    configure(cm::sim::fast_test_config());
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const auto report = co_await pod->dispense(kDispenser, 0.4 * units::milli_litre);
        CHECK(report.amount == 0.0 * units::milli_litre);
        CHECK(report.outcome == cm::DispenseOutcome::completed);
        CHECK(client->stats().dispense_requests == 0);
    });
}

TEST_CASE_METHOD(SimPodFixture, "A short pour from a dry bottle reports an empty bottle", "[sim][pod]")
{
    configure(cm::sim::fast_test_config());
    client->bottle(kDispenser).liquid = 30 * units::si::gram;
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const auto report = co_await pod->dispense(kDispenser, 100 * units::milli_litre);
        CHECK(report.amount == 30 * units::milli_litre);
        CHECK(report.outcome == cm::DispenseOutcome::bottle_empty);
    });
}

TEST_CASE_METHOD(SimPodFixture, "A pour over the time limit reports the limit", "[sim][pod]")
{
    auto config = cm::sim::fast_test_config();
    config.pour_limit = 200ms;
    config.flow = 100.0 * units::milli_litre / units::si::second;
    configure(config);
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const auto report = co_await pod->dispense(kDispenser, 100 * units::milli_litre);
        CHECK(report.outcome == cm::DispenseOutcome::timed_out);
    });
}

TEST_CASE_METHOD(SimPodFixture, "A pour that used up its trims reports still short", "[sim][pod]")
{
    auto config = cm::sim::fast_test_config();
    config.trim_shortfall = 3 * units::si::gram;
    configure(config);
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const auto report = co_await pod->dispense(kDispenser, 40 * units::milli_litre);
        CHECK(report.amount == 37 * units::milli_litre);
        CHECK(report.outcome == cm::DispenseOutcome::still_short);
    });
}

TEST_CASE_METHOD(SimPodFixture, "A tare that takes too long isn't sent again", "[sim][pod]")
{
    auto config = cm::sim::fast_test_config();
    config.load_cell_action = 3s;
    configure(config);
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        REQUIRE_THROWS_AS(co_await pod->load_cell_tare(kDispenser), cm::PodTimeoutError);
        CHECK(client->stats().tare_requests == 1);
    });
}

TEST_CASE_METHOD(SimPodFixture, "A refused pour fails at once instead of waiting for the final reply", "[sim][pod]")
{
    auto config = cm::sim::fast_test_config();
    config.load_cell_action = 300ms;
    configure(config);
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const auto refused_pour = [this]() -> cm::Task<void> {
            const auto start = std::chrono::steady_clock::now();
            try {
                std::ignore = co_await pod->dispense(kDispenser, 5 * units::milli_litre);
                FAIL("Expected a PodReceiveError");
            }
            catch (const cm::PodReceiveError& error) {
                CHECK(error.error_code() == ErrorCode::Busy);
            }
            CHECK(std::chrono::steady_clock::now() - start < 1s);
        };

        std::vector<cm::Task<void>> tasks;
        tasks.emplace_back(pod->load_cell_tare(kDispenser));
        tasks.emplace_back(refused_pour());
        co_await cm::join_all(std::move(tasks));
    });
}

TEST_CASE_METHOD(SimPodFixture, "A run with next to nothing coming out is reported as empty", "[sim][pod]")
{
    configure(cm::sim::fast_test_config());
    client->bottle(kDispenser).liquid = 3 * units::si::gram;
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        REQUIRE_THROWS_AS(co_await pod->dispenser_calibrate(kDispenser, 200ms), cm::DispenserEmptyError);
    });
}

TEST_CASE_METHOD(SimPodFixture, "An emergency stop fails the running pours with a hardware fault", "[sim][pod]")
{
    auto config = cm::sim::fast_test_config();
    config.flow = 10.0 * units::milli_litre / units::si::second;
    configure(config);
    run_test([this]() -> cm::Task<void> {
        co_await connected();

        const auto pour = [this](cm::DispenserId dispenser) -> cm::Task<void> {
            try {
                std::ignore = co_await pod->dispense(dispenser, 200 * units::milli_litre);
                FAIL("Expected a PodReceiveError");
            }
            catch (const cm::PodReceiveError& error) {
                CHECK(error.error_code() == ErrorCode::HardwareFault);
            }
        };
        const auto stop = [this]() -> cm::Task<void> {
            co_await cm::async_sleep(ctx.get_executor(), 200ms);
            co_await pod->force_safe_state();
        };

        std::vector<cm::Task<void>> tasks;
        tasks.emplace_back(pour(kDispenser));
        tasks.emplace_back(pour(kOtherDispenser));
        tasks.emplace_back(stop());
        co_await cm::join_all(std::move(tasks));
    });
}

TEST_CASE_METHOD(SimPodFixture, "A pour at real speed finishes within the timeouts", "[sim][pod]")
{
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        // At 4 mL/s and with the load settling for 1.5 s, the final reply comes well after the Ack.
        const auto report = co_await pod->dispense(kDispenser, 5 * units::milli_litre);
        CHECK(report.amount == 5 * units::milli_litre);
        CHECK(report.outcome == cm::DispenseOutcome::completed);
    });
}

// It takes over 30 s, so it only runs when asked for by its tag.
TEST_CASE_METHOD(SimPodFixture, "A pour at the real time limit stays within the timeout", "[.slow][sim][pod]")
{
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const auto report = co_await pod->dispense(kDispenser, 170 * units::milli_litre);
        CHECK(report.outcome == cm::DispenseOutcome::timed_out);
    });
}

TEST_CASE_METHOD(SimPodFixture, "An Ack that comes too late fails the pour without sending it again", "[sim][pod]")
{
    auto config = cm::sim::fast_test_config();
    config.ack_delay = 700ms;
    configure(config);
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        REQUIRE_THROWS_AS(co_await pod->dispense(kDispenser, 5 * units::milli_litre), cm::PodTimeoutError);
        CHECK(client->stats().dispense_requests == 1);
    });
}

TEST_CASE_METHOD(SimPodFixture, "An emergency stop fails a running calibration", "[sim][pod]")
{
    auto config = cm::sim::fast_test_config();
    config.flow = 10.0 * units::milli_litre / units::si::second;
    configure(config);
    run_test([this]() -> cm::Task<void> {
        co_await connected();

        const auto run = [this]() -> cm::Task<void> {
            try {
                std::ignore = co_await pod->dispenser_calibrate(kDispenser, 5s);
                FAIL("Expected a PodReceiveError");
            }
            catch (const cm::PodReceiveError& error) {
                CHECK(error.error_code() == ErrorCode::HardwareFault);
            }
        };
        const auto stop = [this]() -> cm::Task<void> {
            co_await cm::async_sleep(ctx.get_executor(), 200ms);
            co_await pod->force_safe_state();
        };

        std::vector<cm::Task<void>> tasks;
        tasks.emplace_back(run());
        tasks.emplace_back(stop());
        co_await cm::join_all(std::move(tasks));
    });
}

namespace {
struct SimFillLevelFixture : SimPodFixture
{
    cm::testing::TempDir dir{"cm-sim-fill-levels-test"};
    cm::FillLevelTracker fill_levels{
        ctx.get_executor(), dir.file("levels.json"), cm::FillLevelOptions{.refill_settle_time = 1ms}};
    cm::PodRegistry registry;
    const cm::PodInfo info{.id = cm::PodId{"SimPod"}, .firmware_version = {}, .num_pumps = 5, .num_valves = 3};
    const cm::PodDispenser address{info.id, kDispenser};

    SimFillLevelFixture()
    {
        configure(cm::sim::fast_test_config());
        registry.register_pod(pod);
    }

    ~SimFillLevelFixture()
    {
        registry.unregister_pod(pod);
    }

    cm::Dispenser& dispenser()
    {
        auto created = registry.dispenser_of_pod(address.pod_id, address.dispenser_id);
        REQUIRE(created.has_value());
        held_dispenser = std::move(*created);
        return *held_dispenser;
    }

    std::unique_ptr<cm::Dispenser> held_dispenser;
};
} // namespace

TEST_CASE_METHOD(SimFillLevelFixture, "A connecting pod gets its levels corrected after a power loss", "[sim][fill_level]")
{
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        fill_levels.empty_bottle_tared(address);
        client->bottle(kDispenser).liquid += 300 * units::si::gram;
        co_await fill_levels.refresh(address, dispenser());
        REQUIRE(fill_levels.record(address)->level == 300 * units::si::gram);

        fill_levels.pod_disconnected(info.id);
        client->power_cycle_load_cells();
        co_await cm::sync_pod_fill_levels(info, registry, fill_levels);
        CHECK(fill_levels.record(address)->level == 300 * units::si::gram);

        client->bottle(kDispenser).liquid -= 100 * units::si::gram;
        co_await fill_levels.refresh(address, dispenser());
        CHECK(fill_levels.record(address)->level == 200 * units::si::gram);
    });
}

TEST_CASE_METHOD(SimFillLevelFixture,
                 "A dispenser without a load cell doesn't disturb the others at connect",
                 "[sim][fill_level]")
{
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const cm::PodDispenser without_cell{info.id, kWithoutLoadCell};
        fill_levels.empty_bottle_tared(without_cell);
        fill_levels.empty_bottle_tared(cm::PodDispenser{info.id, cm::DispenserId{5}});

        co_await cm::sync_pod_fill_levels(info, registry, fill_levels);

        CHECK(fill_levels.record(without_cell)->level == 0 * units::si::gram);
        CHECK(fill_levels.view(without_cell).no_load_cell);
        CHECK_FALSE(fill_levels.view(cm::PodDispenser{info.id, cm::DispenserId{5}}).no_load_cell);
        CHECK(client->stats().weight_requests == 8);
    });
}

TEST_CASE_METHOD(SimFillLevelFixture, "The weights at connect are read one after the other", "[sim][fill_level]")
{
    auto config = cm::sim::fast_test_config();
    config.quick_reply = 50ms;
    registry.unregister_pod(pod);
    configure(config);
    registry.register_pod(pod);
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        for (int i = 0; i < 4; ++i) {
            fill_levels.empty_bottle_tared(cm::PodDispenser{info.id, cm::DispenserId{i}});
        }

        co_await cm::sync_pod_fill_levels(info, registry, fill_levels);

        CHECK(client->stats().weight_requests == 8);
        CHECK(client->stats().max_outstanding_weight_requests == 1);
    });
}

TEST_CASE_METHOD(SimFillLevelFixture, "A pod that is gone during the reads ends them quietly", "[sim][fill_level]")
{
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        fill_levels.empty_bottle_tared(address);
        registry.unregister_pod(pod);

        co_await cm::sync_pod_fill_levels(info, registry, fill_levels);

        CHECK(client->stats().weight_requests == 0);
        registry.register_pod(pod);
    });
}

namespace {
class SimApplication : public cm::Application
{
  public:
    using Application::run;
};

class SilentStationState : public cm::StationState
{
    class State final : public cm::PodState
    {
      public:
        void update_info(cm::PodInfo /*info*/) override
        {
        }

        void update_state(ConnectionState /*state*/) override
        {
        }
    };

  public:
    std::unique_ptr<cm::PodState> create_pod_state() override
    {
        return std::make_unique<State>();
    }
};
} // namespace

TEST_CASE_METHOD(SimFillLevelFixture, "A connection lost during the reads ends them without an error", "[sim][fill_level]")
{
    auto config = cm::sim::fast_test_config();
    config.quick_reply = 50ms;
    registry.unregister_pod(pod);
    configure(config);
    registry.register_pod(pod);
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        for (int i = 0; i < 4; ++i) {
            fill_levels.empty_bottle_tared(cm::PodDispenser{info.id, cm::DispenserId{i}});
        }

        const auto lose_connection = [this]() -> cm::Task<void> {
            co_await cm::async_sleep(ctx.get_executor(), 70ms);
            client->socket().close();
        };
        std::vector<cm::Task<void>> tasks;
        tasks.emplace_back(cm::sync_pod_fill_levels(info, registry, fill_levels));
        tasks.emplace_back(lose_connection());
        co_await cm::join_all(std::move(tasks));
    });
}

TEST_CASE("The application reads the fill levels of a pod that connects", "[sim][fill_level]")
{
    // The application keeps its file in the working directory, and this test must not touch a real one.
    const auto directory = std::filesystem::temp_directory_path() /
                           std::format("sim-application-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(directory);
    const auto previous_directory = std::filesystem::current_path();
    std::filesystem::current_path(directory);

    struct RestoreDirectory
    {
        std::filesystem::path previous;
        std::filesystem::path temporary;

        ~RestoreDirectory()
        {
            std::filesystem::current_path(previous);
            std::filesystem::remove_all(temporary);
        }
    } restore_directory{previous_directory, directory};

    std::atomic<int> changes{0};
    SimApplication app;
    app.fill_levels().set_change_handler([&changes](const cm::PodDispenser&, const cm::FillLevelView&) { ++changes; });

    app.run(std::make_shared<SilentStationState>(), std::make_unique<cm::sim::SimulatedPodDiscovery>());

    // Two simulated pods have eight dispensers each.
    const auto end = std::chrono::steady_clock::now() + 5s;
    while (changes < 16 && std::chrono::steady_clock::now() < end) {
        std::this_thread::sleep_for(10ms);
    }
    CHECK(changes == 16);
    app.shutdown();
}

TEST_CASE_METHOD(SimPodFixture, "A pod on another protocol release is incompatible and pours nothing", "[sim][pod]")
{
    auto config = cm::sim::fast_test_config();
    config.protocol_version = cm::Version{.major = 2, .minor = 2, .patch = 0};
    configure(config);
    run_test([this]() -> cm::Task<void> {
        co_await is_incompatible.async_wait();

        // The Pod knows who it is before it checks the release, so the log and the UI can name it.
        CHECK(pod->pod_id() == cm::PodId{"SimPod"});
        CHECK_FALSE(pod->create_dispenser(kDispenser).has_value());
        CHECK_FALSE(static_cast<bool>(is_connected));
    });
}

TEST_CASE("The Station speaks the protocol it was built against", "[sim][pod]")
{
    CHECK(cm::station_protocol_version() == cm::Version{.major = 2, .minor = 1, .patch = 0});
    CHECK(std::format("{}", cm::station_protocol_version()) == "2.1.0");
}

TEST_CASE_METHOD(SimPodFixture, "Progress during a pour doesn't get in the way of its result", "[sim][pod]")
{
    auto config = cm::sim::fast_test_config();
    config.flow = 100.0 * units::milli_litre / units::si::second;
    config.progress_interval = 1ms;
    configure(config);
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const auto report = co_await pod->dispense(kDispenser, 100 * units::milli_litre);

        CHECK(report.outcome == cm::DispenseOutcome::completed);
        CHECK(report.amount == 100 * units::milli_litre);
        CHECK(client->stats().progress_reports >= 5);
    });
}
