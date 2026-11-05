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
using Clock = std::chrono::steady_clock;
using ErrorCode = proto::field::ErrorCodeVal;
namespace units = cm::units;

constexpr auto kReplyTimeout = 5s;
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

cm::OutLoadCellTare tare(cm::DispenserId id)
{
    cm::OutLoadCellTare msg;
    msg.field_dispenserId().setValue(id.raw());
    return msg;
}

cm::OutLoadCellCalibrateWithRefWeight reference(cm::DispenserId id, int grams)
{
    cm::OutLoadCellCalibrateWithRefWeight msg;
    msg.field_dispenserId().setValue(id.raw());
    msg.field_gram().setValue(grams);
    return msg;
}

cm::OutDispense dispense(cm::DispenserId id, int millilitre)
{
    cm::OutDispense msg;
    msg.field_dispenserId().setValue(id.raw());
    msg.field_millilitre().setValue(millilitre);
    return msg;
}

cm::OutDispenserStartCalibration calibration(cm::DispenserId id, std::chrono::milliseconds run_time)
{
    cm::OutDispenserStartCalibration msg;
    msg.field_dispenserId().setValue(id.raw());
    msg.field_milliseconds().setValue(run_time.count());
    return msg;
}

ErrorCode error_of(const std::variant<cm::InAck, cm::InNak>& reply)
{
    REQUIRE(std::holds_alternative<cm::InNak>(reply));
    return std::get<cm::InNak>(reply).field_errorCode().value();
}

template <typename Final>
ErrorCode error_of(const std::variant<Final, cm::InNak>& reply)
{
    REQUIRE(std::holds_alternative<cm::InNak>(reply));
    return std::get<cm::InNak>(reply).field_errorCode().value();
}

/// Talks to a simulated client through a plain session, so the tests don't depend on the timeouts of `cm::Pod`.
struct ClientFixture
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    std::shared_ptr<cm::sim::Client<Socket>> client;
    std::unique_ptr<cm::PodProtocolSession> session;

    void create(cm::sim::ClientConfig config = fast_config())
    {
        client = std::make_shared<cm::sim::Client<Socket>>(scope, "SimPod", cm::Version{.major = 2}, std::move(config));
        Socket session_socket{ctx};
        boost::asio::local::connect_pair(client->socket(), session_socket);
        session =
            std::make_unique<cm::PodProtocolSession>(std::make_unique<cm::SocketIoStream<Socket>>(std::move(session_socket)));
    }

    template <typename TxMsg>
    cm::Task<std::variant<cm::InAck, cm::InNak>> ack_of(TxMsg msg)
    {
        const auto id = session->generate_new_transaction_id();
        auto reply = session->async_receive<cm::InAck, cm::InNak>(id, kReplyTimeout);
        co_await session->async_send(std::move(msg), id);
        co_return co_await std::move(reply);
    }

    cm::Task<int> weight_of(cm::DispenserId dispenser)
    {
        cm::OutLoadCellWeightRequest msg;
        msg.field_dispenserId().setValue(dispenser.raw());
        const auto id = session->generate_new_transaction_id();
        auto reply = session->async_receive<cm::InLoadCellWeightResponse>(id, kReplyTimeout);
        co_await session->async_send(std::move(msg), id);
        const auto response = co_await std::move(reply);
        co_return response.field_gram().value();
    }

    /// Sends a command that ends with a message of its own, and waits for its Ack.
    template <typename Final, typename TxMsg>
    cm::Task<std::variant<Final, cm::InNak>> action_of(TxMsg msg)
    {
        const auto id = session->generate_new_transaction_id();
        auto ack = session->async_receive<cm::InAck, cm::InNak>(id, kReplyTimeout);
        auto final_reply = session->async_receive<Final, cm::InNak>(id, kReplyTimeout);
        co_await session->async_send(std::move(msg), id);

        const auto first = co_await std::move(ack);
        if (std::holds_alternative<cm::InNak>(first)) {
            co_return std::get<cm::InNak>(first);
        }
        co_return co_await std::move(final_reply);
    }

    /// Fails if another reply to this transaction arrives within `duration`.
    cm::Task<void> expect_no_reply(cm::TransactionId::ValueType id, std::chrono::milliseconds duration)
    {
        auto reply = session->async_receive<cm::InDispenseFinished, cm::InNak>(id, duration);
        REQUIRE_THROWS_AS(co_await std::move(reply), cm::TimeoutError);
    }

    cm::Task<void> pause(std::chrono::milliseconds duration)
    {
        boost::asio::steady_timer timer{ctx, duration};
        co_await timer.async_wait(exec::asio::use_sender);
    }

    template <typename CoroutineFunc>
    void run_test(CoroutineFunc&& test_coro)
    {
        std::exception_ptr error;
        auto test_wrapper = [&]() -> cm::Task<void> {
            try {
                co_await exec::when_any(test_coro(), session->run());
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

TEST_CASE_METHOD(ClientFixture, "A tare takes as long as on the real Pod", "[sim][client]")
{
    create(cm::sim::ClientConfig{});
    run_test([this]() -> cm::Task<void> {
        const auto start = Clock::now();
        const auto reply = co_await ack_of(tare(kDispenser));
        const auto elapsed = Clock::now() - start;

        CHECK(std::holds_alternative<cm::InAck>(reply));
        CHECK(elapsed >= 1200ms);
        CHECK(elapsed <= 1900ms);
    });
}

TEST_CASE_METHOD(ClientFixture, "A second tare is refused while the first one runs", "[sim][client]")
{
    auto config = fast_config();
    config.load_cell_action = 300ms;
    create(config);
    run_test([this]() -> cm::Task<void> {
        const auto id = session->generate_new_transaction_id();
        auto first = session->async_receive<cm::InAck, cm::InNak>(id, kReplyTimeout);
        co_await session->async_send(tare(kDispenser), id);

        const auto second = co_await ack_of(tare(kDispenser));
        CHECK(error_of(second) == ErrorCode::Busy);

        const auto first_reply = co_await std::move(first);
        CHECK(std::holds_alternative<cm::InAck>(first_reply));
    });
}

TEST_CASE_METHOD(ClientFixture, "The weight is relative to the zero point", "[sim][client]")
{
    create();
    run_test([this]() -> cm::Task<void> {
        CHECK(co_await weight_of(kDispenser) == 0);

        std::ignore = co_await action_of<cm::InDispenseFinished>(dispense(kDispenser, 100));
        CHECK(co_await weight_of(kDispenser) == -100);

        std::ignore = co_await ack_of(tare(kDispenser));
        CHECK(co_await weight_of(kDispenser) == 0);
    });
}

TEST_CASE_METHOD(ClientFixture, "A power cycle of the load cells moves the zero point", "[sim][client]")
{
    create();
    run_test([this]() -> cm::Task<void> {
        std::ignore = co_await action_of<cm::InDispenseFinished>(dispense(kDispenser, 100));
        client->power_cycle_load_cells();
        CHECK(co_await weight_of(kDispenser) == 0);
    });
}

TEST_CASE_METHOD(ClientFixture, "A reference calibration needs a recent tare", "[sim][client]")
{
    create();
    run_test([this]() -> cm::Task<void> {
        const auto without_tare = co_await ack_of(reference(kDispenser, 100));
        CHECK(error_of(without_tare) == ErrorCode::TareTooOld);

        std::ignore = co_await ack_of(tare(kDispenser));
        const auto with_tare = co_await ack_of(reference(kDispenser, 100));
        CHECK(std::holds_alternative<cm::InAck>(with_tare));
    });
}

TEST_CASE_METHOD(ClientFixture, "A reference weight of 0 g is invalid", "[sim][client]")
{
    create();
    run_test([this]() -> cm::Task<void> {
        std::ignore = co_await ack_of(tare(kDispenser));
        const auto reply = co_await ack_of(reference(kDispenser, 0));
        CHECK(error_of(reply) == ErrorCode::InvalidParameter);
    });
}

TEST_CASE_METHOD(ClientFixture, "A pour from a bottle running dry reports what came out", "[sim][client]")
{
    create();
    client->bottle(kDispenser).liquid = 30 * units::si::gram;
    run_test([this]() -> cm::Task<void> {
        const auto reply = co_await action_of<cm::InDispenseFinished>(dispense(kDispenser, 100));
        REQUIRE(std::holds_alternative<cm::InDispenseFinished>(reply));
        CHECK(std::get<cm::InDispenseFinished>(reply).field_millilitre().value() == 30);
    });
}

TEST_CASE_METHOD(ClientFixture, "A pour of 0 mL is invalid", "[sim][client]")
{
    create();
    run_test([this]() -> cm::Task<void> {
        const auto reply = co_await ack_of(dispense(kDispenser, 0));
        CHECK(error_of(reply) == ErrorCode::InvalidParameter);
    });
}

TEST_CASE_METHOD(ClientFixture, "A pour stops at the time limit", "[sim][client]")
{
    auto config = fast_config();
    config.pour_limit = 200ms;
    config.flow = 100.0 * units::milli_litre / units::si::second;
    create(config);
    run_test([this]() -> cm::Task<void> {
        const auto reply = co_await action_of<cm::InDispenseFinished>(dispense(kDispenser, 100));
        REQUIRE(std::holds_alternative<cm::InDispenseFinished>(reply));
        const auto amount = std::get<cm::InDispenseFinished>(reply).field_millilitre().value();
        CHECK(amount > 0);
        CHECK(amount < 100);
    });
}

TEST_CASE_METHOD(ClientFixture, "A blocked hose ends a pour with nothing poured", "[sim][client]")
{
    create();
    client->bottle(kDispenser).hose_blocked = true;
    run_test([this]() -> cm::Task<void> {
        const auto reply = co_await action_of<cm::InDispenseFinished>(dispense(kDispenser, 100));
        REQUIRE(std::holds_alternative<cm::InDispenseFinished>(reply));
        CHECK(std::get<cm::InDispenseFinished>(reply).field_millilitre().value() == 0);
        CHECK(client->bottle(kDispenser).liquid == 500 * units::si::gram);
    });
}

TEST_CASE_METHOD(ClientFixture, "A calibration run answers like the firmware", "[sim][client]")
{
    create();
    using Finished = cm::InDispenserFinishedCalibrationResponse;
    run_test([this]() -> cm::Task<void> {
        const auto too_short = co_await action_of<Finished>(calibration(kDispenser, 0ms));
        CHECK(error_of(too_short) == ErrorCode::InvalidParameter);

        const auto too_long = co_await action_of<Finished>(calibration(kDispenser, 30001ms));
        CHECK(error_of(too_long) == ErrorCode::InvalidParameter);

        const auto good = co_await action_of<Finished>(calibration(kDispenser, 200ms));
        REQUIRE(std::holds_alternative<Finished>(good));
        CHECK(std::get<Finished>(good).field_millilitre().value() == 200);
    });
}

TEST_CASE_METHOD(ClientFixture, "A calibration run below the smallest amount counts as empty", "[sim][client]")
{
    auto config = fast_config();
    config.stall = 5s;
    config.flow = 20.0 * units::milli_litre / units::si::second;
    create(config);
    using Finished = cm::InDispenserFinishedCalibrationResponse;
    run_test([this]() -> cm::Task<void> {
        const auto too_little = co_await action_of<Finished>(calibration(kDispenser, 200ms));
        CHECK(error_of(too_little) == ErrorCode::DispenserEmpty);
    });
}

TEST_CASE_METHOD(ClientFixture, "A calibration run that stalls counts as empty even with enough poured", "[sim][client]")
{
    create();
    client->bottle(kDispenser).liquid = 10 * units::si::gram;
    using Finished = cm::InDispenserFinishedCalibrationResponse;
    run_test([this]() -> cm::Task<void> {
        const auto stalled = co_await action_of<Finished>(calibration(kDispenser, 2s));
        CHECK(error_of(stalled) == ErrorCode::DispenserEmpty);
    });
}

TEST_CASE_METHOD(ClientFixture, "A running pour blocks only its own dispenser", "[sim][client]")
{
    auto config = fast_config();
    config.flow = 100.0 * units::milli_litre / units::si::second;
    create(config);
    run_test([this]() -> cm::Task<void> {
        const auto id = session->generate_new_transaction_id();
        auto ack = session->async_receive<cm::InAck, cm::InNak>(id, kReplyTimeout);
        auto running = session->async_receive<cm::InDispenseFinished, cm::InNak>(id, kReplyTimeout);
        co_await session->async_send(dispense(kDispenser, 40), id);
        const auto ack_reply = co_await std::move(ack);
        REQUIRE(std::holds_alternative<cm::InAck>(ack_reply));

        const auto tare_reply = co_await ack_of(tare(kDispenser));
        CHECK(error_of(tare_reply) == ErrorCode::Busy);
        const auto pour_reply = co_await ack_of(dispense(kDispenser, 5));
        CHECK(error_of(pour_reply) == ErrorCode::Busy);
        const auto calibration_reply = co_await ack_of(calibration(kDispenser, 100ms));
        CHECK(error_of(calibration_reply) == ErrorCode::Busy);

        const auto other = co_await action_of<cm::InDispenseFinished>(dispense(kOtherDispenser, 5));
        CHECK(std::holds_alternative<cm::InDispenseFinished>(other));
        const auto other_tare = co_await ack_of(tare(kOtherDispenser));
        CHECK(std::holds_alternative<cm::InAck>(other_tare));
        const auto other_run =
            co_await action_of<cm::InDispenserFinishedCalibrationResponse>(calibration(kOtherDispenser, 100ms));
        CHECK(std::holds_alternative<cm::InDispenserFinishedCalibrationResponse>(other_run));

        const auto finished = co_await std::move(running);
        CHECK(std::holds_alternative<cm::InDispenseFinished>(finished));
    });
}

TEST_CASE_METHOD(ClientFixture, "A dispenser without a load cell refuses everything that needs one", "[sim][client]")
{
    create();
    using Finished = cm::InDispenserFinishedCalibrationResponse;
    run_test([this]() -> cm::Task<void> {
        const auto tare_reply = co_await ack_of(tare(kWithoutLoadCell));
        CHECK(error_of(tare_reply) == ErrorCode::NoLoadCell);
        const auto reference_reply = co_await ack_of(reference(kWithoutLoadCell, 100));
        CHECK(error_of(reference_reply) == ErrorCode::NoLoadCell);
        const auto pour_reply = co_await ack_of(dispense(kWithoutLoadCell, 5));
        CHECK(error_of(pour_reply) == ErrorCode::NoLoadCell);
        const auto calibration_reply = co_await action_of<Finished>(calibration(kWithoutLoadCell, 100ms));
        CHECK(error_of(calibration_reply) == ErrorCode::NoLoadCell);

        cm::OutLoadCellWeightRequest weight;
        weight.field_dispenserId().setValue(kWithoutLoadCell.raw());
        const auto id = session->generate_new_transaction_id();
        auto reply = session->async_receive<cm::InLoadCellWeightResponse, cm::InNak>(id, kReplyTimeout);
        co_await session->async_send(std::move(weight), id);
        const auto weight_reply = co_await std::move(reply);
        CHECK(error_of(weight_reply) == ErrorCode::NoLoadCell);
    });
}

TEST_CASE_METHOD(ClientFixture, "An emergency stop acknowledges first and ends every running pour", "[sim][client]")
{
    auto config = fast_config();
    config.flow = 10.0 * units::milli_litre / units::si::second;
    config.load_cell_action = 300ms;
    config.stop_nak_spacing = 200ms;
    create(config);
    run_test([this]() -> cm::Task<void> {
        const auto start_pour = [this](cm::DispenserId dispenser) -> cm::Task<cm::TransactionId::ValueType> {
            const auto id = session->generate_new_transaction_id();
            auto ack = session->async_receive<cm::InAck, cm::InNak>(id, kReplyTimeout);
            co_await session->async_send(dispense(dispenser, 200), id);
            const auto reply = co_await std::move(ack);
            REQUIRE(std::holds_alternative<cm::InAck>(reply));
            co_return id;
        };
        const auto first_id = co_await start_pour(kDispenser);
        auto first_final = session->async_receive<cm::InDispenseFinished, cm::InNak>(first_id, kReplyTimeout);
        const auto second_id = co_await start_pour(kOtherDispenser);
        auto second_final = session->async_receive<cm::InDispenseFinished, cm::InNak>(second_id, kReplyTimeout);

        const cm::DispenserId tared{2};
        const auto tare_id = session->generate_new_transaction_id();
        auto tare_reply = session->async_receive<cm::InAck, cm::InNak>(tare_id, kReplyTimeout);
        co_await session->async_send(tare(tared), tare_id);

        const auto stop_reply = co_await ack_of(cm::OutEmergencyStop{});
        CHECK(std::holds_alternative<cm::InAck>(stop_reply));

        const auto first_reply = co_await std::move(first_final);
        const auto first_at = Clock::now();
        CHECK(error_of(first_reply) == ErrorCode::HardwareFault);

        const auto second_reply = co_await std::move(second_final);
        CHECK(Clock::now() - first_at >= 100ms);
        CHECK(error_of(second_reply) == ErrorCode::HardwareFault);

        const auto tare_result = co_await std::move(tare_reply);
        CHECK(std::holds_alternative<cm::InAck>(tare_result));
    });
}

TEST_CASE_METHOD(ClientFixture, "A dry bottle refills itself when configured", "[sim][client]")
{
    auto config = fast_config();
    config.auto_refill_after = 50ms;
    create(config);
    run_test([this]() -> cm::Task<void> {
        const auto reply = co_await action_of<cm::InDispenseFinished>(dispense(kDispenser, 600));
        REQUIRE(std::holds_alternative<cm::InDispenseFinished>(reply));
        CHECK(std::get<cm::InDispenseFinished>(reply).field_millilitre().value() == 500);
        CHECK(client->bottle(kDispenser).liquid == 0 * units::si::gram);

        co_await pause(150ms);
        CHECK(client->bottle(kDispenser).liquid == 500 * units::si::gram);
    });
}

TEST_CASE_METHOD(ClientFixture, "A stop before the Ack ends a pour with a single reply", "[sim][client]")
{
    auto config = fast_config();
    config.ack_delay = 200ms;
    create(config);
    run_test([this]() -> cm::Task<void> {
        const auto id = session->generate_new_transaction_id();
        auto first = session->async_receive<cm::InAck, cm::InNak>(id, kReplyTimeout);
        co_await session->async_send(dispense(kDispenser, 50), id);

        co_await pause(50ms);
        std::ignore = co_await ack_of(cm::OutEmergencyStop{});

        const auto reply = co_await std::move(first);
        CHECK(error_of(reply) == ErrorCode::HardwareFault);
        co_await expect_no_reply(id, 500ms);
        CHECK(client->bottle(kDispenser).liquid == 500 * units::si::gram);
    });
}

TEST_CASE_METHOD(ClientFixture, "A stop while the load settles ends a pour with a single reply", "[sim][client]")
{
    auto config = fast_config();
    config.settle = 400ms;
    create(config);
    run_test([this]() -> cm::Task<void> {
        const auto id = session->generate_new_transaction_id();
        auto ack = session->async_receive<cm::InAck, cm::InNak>(id, kReplyTimeout);
        auto final_reply = session->async_receive<cm::InDispenseFinished, cm::InNak>(id, kReplyTimeout);
        co_await session->async_send(dispense(kDispenser, 50), id);
        const auto ack_reply = co_await std::move(ack);
        REQUIRE(std::holds_alternative<cm::InAck>(ack_reply));

        // The first tick pours everything, and the load settles after that.
        co_await pause(200ms);
        std::ignore = co_await ack_of(cm::OutEmergencyStop{});

        const auto reply = co_await std::move(final_reply);
        CHECK(error_of(reply) == ErrorCode::HardwareFault);
        co_await expect_no_reply(id, 600ms);
    });
}

TEST_CASE_METHOD(ClientFixture, "A stop during a calibration run ends it with a single reply", "[sim][client]")
{
    auto config = fast_config();
    config.flow = 100.0 * units::milli_litre / units::si::second;
    create(config);
    run_test([this]() -> cm::Task<void> {
        const auto id = session->generate_new_transaction_id();
        auto ack = session->async_receive<cm::InAck, cm::InNak>(id, kReplyTimeout);
        auto final_reply = session->async_receive<cm::InDispenserFinishedCalibrationResponse, cm::InNak>(id, kReplyTimeout);
        co_await session->async_send(calibration(kDispenser, 600ms), id);
        const auto ack_reply = co_await std::move(ack);
        REQUIRE(std::holds_alternative<cm::InAck>(ack_reply));

        co_await pause(200ms);
        std::ignore = co_await ack_of(cm::OutEmergencyStop{});

        const auto reply = co_await std::move(final_reply);
        CHECK(error_of(reply) == ErrorCode::HardwareFault);

        const auto liquid_at_stop = client->bottle(kDispenser).liquid;
        auto later = session->async_receive<cm::InDispenserFinishedCalibrationResponse, cm::InNak>(id, 800ms);
        REQUIRE_THROWS_AS(co_await std::move(later), cm::TimeoutError);
        CHECK(client->bottle(kDispenser).liquid == liquid_at_stop);
    });
}

TEST_CASE_METHOD(ClientFixture, "An old refill doesn't fill a bottle that is in use again", "[sim][client]")
{
    auto config = fast_config();
    config.auto_refill_after = 150ms;
    create(config);
    client->bottle(kDispenser).liquid = 10 * units::si::gram;
    run_test([this]() -> cm::Task<void> {
        std::ignore = co_await action_of<cm::InDispenseFinished>(dispense(kDispenser, 20));
        REQUIRE(client->bottle(kDispenser).liquid == 0 * units::si::gram);

        // Somebody refills by hand before the automatic refill is due.
        client->bottle(kDispenser).liquid = 40 * units::si::gram;
        co_await pause(300ms);
        CHECK(client->bottle(kDispenser).liquid == 40 * units::si::gram);
    });
}
