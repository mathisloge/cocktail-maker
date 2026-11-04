#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <exec/asio/use_sender.hpp>
#include <exec/when_any.hpp>
#include <proto/field/ErrorCodeCommon.h>
#include <stdexec/execution.hpp>

import std;
import mp_units;
import cm.core;
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

cm::sim::ClientConfig fast_config()
{
    return cm::sim::ClientConfig{
        .load_cell_action = 50ms,
        .settle = 50ms,
        .stall = 50ms,
        .flow = 1000.0 * units::milli_litre / units::si::second,
    };
}

class TestPodState final : public cm::PodState
{
  public:
    explicit TestPodState(cm::AwaitableBool& connected)
        : connected_{connected}
    {
    }

    void update_info(cm::PodInfo info) override
    {
        info_ = std::move(info);
    }

    cm::PodInfo info() const override
    {
        return info_;
    }

    void update_state(ConnectionState state) override
    {
        if (state == ConnectionState::connected) {
            connected_ = true;
        }
    }

  private:
    cm::AwaitableBool& connected_;
    cm::PodInfo info_;
};

/// A real `cm::Pod` that talks to a simulated client over a socket pair.
struct SimPodFixture
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    cm::AwaitableBool is_connected{ctx.get_executor()};
    std::shared_ptr<cm::sim::Client<Socket>> client =
        std::make_shared<cm::sim::Client<Socket>>(scope, "SimPod", cm::Version{.major = 2});
    std::shared_ptr<cm::Pod> pod = make_pod();

    std::shared_ptr<cm::Pod> make_pod()
    {
        Socket pod_socket{ctx};
        boost::asio::local::connect_pair(client->socket(), pod_socket);
        return std::make_shared<cm::Pod>(std::make_unique<cm::SocketIoStream<Socket>>(std::move(pod_socket)));
    }

    /// Replaces the client and the pod, so it has to be called before `run_test`.
    void configure(cm::sim::ClientConfig config, std::chrono::milliseconds pour_limit = cm::kPodPourLimit)
    {
        client = std::make_shared<cm::sim::Client<Socket>>(scope, "SimPod", cm::Version{.major = 2}, std::move(config));
        Socket pod_socket{ctx};
        boost::asio::local::connect_pair(client->socket(), pod_socket);
        pod = std::make_shared<cm::Pod>(std::make_unique<cm::SocketIoStream<Socket>>(std::move(pod_socket)), pour_limit);
    }

    auto connected()
    {
        return is_connected.async_wait();
    }

    /// Runs `test_coro` while the pod and the client are running, and rethrows what it failed with.
    template <typename CoroutineFunc>
    void run_test(CoroutineFunc&& test_coro)
    {
        std::exception_ptr error;
        auto test_wrapper = [&]() -> cm::Task<void> {
            try {
                co_await exec::when_any(test_coro(), pod->run(std::make_unique<TestPodState>(is_connected)));
            }
            catch (...) {
                error = std::current_exception();
            }
        };

        scope.spawn(client->run());
        cm::run_until_complete(ctx, test_wrapper());

        scope.request_stop();
        cm::run_until_complete(ctx, scope.join());

        if (error) {
            std::rethrow_exception(error);
        }
    }
};
} // namespace

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
    auto config = fast_config();
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
    configure(fast_config());
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
    configure(fast_config());
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        try {
            std::ignore = co_await pod->load_cell_weight(kWithoutLoadCell);
            FAIL("Expected a PodReceiveError");
        }
        catch (const cm::PodReceiveError& error) {
            CHECK(error.error_code() == ErrorCode::HardwareFault);
        }
    });
}

TEST_CASE_METHOD(SimPodFixture, "A volume is rounded to the nearest millilitre", "[sim][pod]")
{
    configure(fast_config());
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        std::ignore = co_await pod->dispense(kDispenser, 19.999 * units::milli_litre);
        CHECK(client->stats().last_dispense_ml == 20);
    });
}

TEST_CASE_METHOD(SimPodFixture, "An amount that rounds to nothing isn't sent", "[sim][pod]")
{
    configure(fast_config());
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
    configure(fast_config());
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
    auto config = fast_config();
    config.pour_limit = 200ms;
    config.flow = 100.0 * units::milli_litre / units::si::second;
    configure(config, 200ms);
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const auto report = co_await pod->dispense(kDispenser, 100 * units::milli_litre);
        CHECK(report.outcome == cm::DispenseOutcome::timed_out);
    });
}

TEST_CASE_METHOD(SimPodFixture, "A tare that takes too long isn't sent again", "[sim][pod]")
{
    auto config = fast_config();
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
    auto config = fast_config();
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
    configure(fast_config());
    client->bottle(kDispenser).liquid = 3 * units::si::gram;
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        REQUIRE_THROWS_AS(co_await pod->dispenser_calibrate(kDispenser, 200ms), cm::DispenserEmptyError);
    });
}

TEST_CASE_METHOD(SimPodFixture, "An emergency stop fails the running pours with a hardware fault", "[sim][pod]")
{
    auto config = fast_config();
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
            boost::asio::steady_timer timer{ctx, 200ms};
            co_await timer.async_wait(exec::asio::use_sender);
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
    auto config = fast_config();
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
    auto config = fast_config();
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
            boost::asio::steady_timer timer{ctx, 200ms};
            co_await timer.async_wait(exec::asio::use_sender);
            co_await pod->force_safe_state();
        };

        std::vector<cm::Task<void>> tasks;
        tasks.emplace_back(run());
        tasks.emplace_back(stop());
        co_await cm::join_all(std::move(tasks));
    });
}
