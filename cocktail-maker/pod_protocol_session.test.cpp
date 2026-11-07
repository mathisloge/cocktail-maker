#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <comms/ErrorStatus.h>
#include <exec/asio/use_sender.hpp>
#include <exec/when_any.hpp>
#include <stdexec/execution.hpp>

import std;
import cm.core;
import cm.testing;
import cm;

using namespace std::chrono_literals;

using Socket = boost::asio::local::stream_protocol::socket;
using Session = cm::PodProtocolSession;

using TestTxMsg = cm::OutPing;
using TestRxMsg = cm::InPong;
using TestClientOutFrame = cm::OutFrame;

constexpr cm::TransactionId::ValueType kTestTransaction = 212;

// Two distinct "wrong id" stand-ins, used to exercise paths where the wire id
// never matches any expected candidate (single or variant). They subclass
// TestRxMsg purely to satisfy the comms interface; the read loop never
// actually constructs these — they only ever appear as template arguments.
struct TestInMsgFake : public TestRxMsg
{
    static constexpr bool hasStaticMsgId()
    {
        return true;
    }

    static constexpr std::uint32_t staticMsgId()
    {
        return 9999;
    }
};

struct AnotherFakeMsg : public TestRxMsg
{
    static constexpr bool hasStaticMsgId()
    {
        return true;
    }

    static constexpr std::uint32_t staticMsgId()
    {
        return 8888;
    }
};

struct PodProtocolSessionTestFixture
{
    boost::asio::io_context ioc;
    Socket client_socket;
    std::unique_ptr<Session> session;

    PodProtocolSessionTestFixture()
        : client_socket(ioc)
    {
        Socket session_socket{ioc};
        boost::asio::local::connect_pair(client_socket, session_socket);
        session = std::make_unique<Session>(std::make_unique<cm::SocketIoStream<Socket>>(std::move(session_socket)));
    }

    /// Runs `test_coro` while the session runs. The session ends with the test, and a failure is rethrown.
    template <typename CoroutineFunc>
    void run_test(CoroutineFunc&& test_coro)
    {
        cm::run_until_complete(ioc, exec::when_any(test_coro(), session->run()));
    }

    // Encodes msg as an on-wire frame tagged with transaction_id. Does not
    // touch the socket — useful when a test needs to control write timing
    // (fragmentation tests) or concatenate several frames (event tests).
    template <typename Msg>
    std::vector<uint8_t> encode_message(Msg msg, cm::TransactionId::ValueType transaction_id)
    {
        msg.transportField_transactionId().setValue(transaction_id);
        TestClientOutFrame frame;
        std::vector<uint8_t> buffer(frame.length(msg));
        auto* iter = buffer.data();
        auto write_es = frame.write(msg, iter, buffer.size());
        REQUIRE(write_es == comms::ErrorStatus::Success);
        return buffer;
    }

    // Encodes and writes msg to the client socket in one shot — the common
    // case for tests that don't care about fragmentation.
    template <typename Msg>
    cm::Task<void> send_message(Msg msg, cm::TransactionId::ValueType transaction_id)
    {
        auto buffer = encode_message(std::move(msg), transaction_id);
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), exec::asio::use_sender);
    }
};

TEST_CASE_METHOD(PodProtocolSessionTestFixture, "PodProtocolSession - Lifecycle Starts and Stops Cleanly", "[lifecycle]")
{
    run_test([&]() -> cm::Task<void> {
        CHECK(true);
        co_return;
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture, "PodProtocolSession - Sends Messages over Socket", "[send]")
{
    run_test([this]() -> cm::Task<void> {
        constexpr cm::TransactionId::ValueType kWideTransaction = 0x1234;

        TestTxMsg msg;
        REQUIRE_NOTHROW(co_await session->async_send(msg, kWideTransaction));

        std::vector<uint8_t> buffer(1024);
        auto [ec, bytes_read] =
            co_await client_socket.async_read_some(boost::asio::buffer(buffer), boost::asio::as_tuple(exec::asio::use_sender));

        REQUIRE(!ec);
        CHECK(bytes_read > 0);

        // Sync, Size and MsgId take 6 bytes, and the frame is little endian.
        CHECK(buffer[6] == 0x34);
        CHECK(buffer[7] == 0x12);
    });
}

// ---------------------------------------------------------------------------
// Single-type async_receive<Msg> -> Msg
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(PodProtocolSessionTestFixture, "PodProtocolSession - Successful Receive", "[receive][success]")
{
    run_test([this]() -> cm::Task<void> {
        // 1. Invoke async_receive first. This registers the channel in dispatch_map_ right away, although the returned task
        //    only starts once it is awaited.
        auto recv_task = session->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        // 2. Safely blast the data over the socket.
        co_await send_message(TestRxMsg{}, kTestTransaction);

        // 3. Await the outcome. The background read_loop will route the incoming frame to recv_task.
        auto res = co_await std::move(recv_task);
        CHECK(res.getId() == TestRxMsg::staticMsgId());
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture, "PodProtocolSession - Receive Fragmented Message", "[receive][fragmented]")
{
    run_test([this]() -> cm::Task<void> {
        auto buffer = encode_message(TestRxMsg{}, kTestTransaction);
        REQUIRE(buffer.size() > 1);
        size_t half = buffer.size() / 2;

        auto recv_task = session->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        // Write first half
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer.data(), half), exec::asio::use_sender);

        // Introduce a slight delay before writing the second half
        co_await cm::async_sleep(ioc.get_executor(), 10ms);

        co_await boost::asio::async_write(
            client_socket, boost::asio::buffer(buffer.data() + half, buffer.size() - half), exec::asio::use_sender);

        auto res = co_await std::move(recv_task);
        REQUIRE(res.transportField_transactionId().getDisplayValue() == kTestTransaction);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture,
                 "PodProtocolSession - Receive Message Fragmented Across Three Writes",
                 "[receive][fragmented]")
{
    run_test([this]() -> cm::Task<void> {
        auto buffer = encode_message(TestRxMsg{}, kTestTransaction);
        REQUIRE(buffer.size() >= 3);
        size_t third = buffer.size() / 3;

        auto recv_task = session->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer.data(), third), exec::asio::use_sender);

        co_await cm::async_sleep(ioc.get_executor(), 5ms);

        co_await boost::asio::async_write(
            client_socket, boost::asio::buffer(buffer.data() + third, third), exec::asio::use_sender);

        co_await cm::async_sleep(ioc.get_executor(), 5ms);

        co_await boost::asio::async_write(
            client_socket, boost::asio::buffer(buffer.data() + 2 * third, buffer.size() - 2 * third), exec::asio::use_sender);

        auto res = co_await std::move(recv_task);
        CHECK(res.transportField_transactionId().getDisplayValue() == kTestTransaction);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture, "PodProtocolSession - Receive Invalid Msg ID", "[receive][invalid_id]")
{
    run_test([this]() -> cm::Task<void> {
        auto recv_task = session->async_receive<TestInMsgFake>(kTestTransaction, std::chrono::milliseconds(500));
        co_await send_message(TestRxMsg{}, kTestTransaction);
        REQUIRE_THROWS_AS(co_await std::move(recv_task), cm::ProtocolError);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture,
                 "PodProtocolSession - Dispatcher Drops Unmatched Transaction IDs",
                 "[receive][dispatch]")
{
    run_test([this]() -> cm::Task<void> {
        auto recv_task1 = session->async_receive<TestRxMsg>(100, std::chrono::milliseconds(500));
        auto recv_task2 = session->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        // Blast identical messages over the socket
        co_await send_message(TestRxMsg{}, kTestTransaction);

        REQUIRE_THROWS_AS(co_await std::move(recv_task1), cm::TimeoutError);
        auto res2 = co_await std::move(recv_task2);
        CHECK(res2.transportField_transactionId().value() == kTestTransaction);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture, "PodProtocolSession - Receive Timeouts", "[receive][timeout]")
{
    run_test([this]() -> cm::Task<void> {
        REQUIRE_THROWS_AS(co_await session->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(10)),
                          cm::TimeoutError);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture,
                 "PodProtocolSession - Channel Cleanup After Timeout Allows Reuse Of Transaction Id",
                 "[receive][cleanup]")
{
    run_test([this]() -> cm::Task<void> {
        // ResponseRegistration must erase the dispatch_map_ entry on the timeout exit
        // path, otherwise this transaction id would stay "stuck" forever.
        REQUIRE_THROWS_AS(co_await session->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(10)),
                          cm::TimeoutError);

        auto recv_task = session->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));
        co_await send_message(TestRxMsg{}, kTestTransaction);
        auto res = co_await std::move(recv_task);
        CHECK(res.transportField_transactionId().value() == kTestTransaction);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture,
                 "PodProtocolSession - Channel Cleanup After Protocol Error Allows Reuse Of Transaction Id",
                 "[receive][cleanup]")
{
    run_test([this]() -> cm::Task<void> {
        // Same as above, but exercising the exception exit path (id mismatch)
        // rather than the timeout exit path.
        auto bad_recv = session->async_receive<TestInMsgFake>(kTestTransaction, std::chrono::milliseconds(500));
        co_await send_message(TestRxMsg{}, kTestTransaction);
        REQUIRE_THROWS_AS(co_await std::move(bad_recv), cm::ProtocolError);

        auto good_recv = session->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));
        co_await send_message(TestRxMsg{}, kTestTransaction);
        auto res = co_await std::move(good_recv);
        CHECK(res.transportField_transactionId().value() == kTestTransaction);
    });
}

// ---------------------------------------------------------------------------
// Variant async_receive<Msgs...> -> std::variant<Msgs...>
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(PodProtocolSessionTestFixture,
                 "PodProtocolSession - Variant Receive Matches A Later Candidate In The Pack",
                 "[receive][variant]")
{
    run_test([this]() -> cm::Task<void> {
        // TestInMsgFake never matches the wire id; this exercises the fold
        // expression correctly skipping a mismatched candidate before
        // landing on the one that actually matches.
        auto recv_task = session->async_receive<TestInMsgFake, TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        co_await send_message(TestRxMsg{}, kTestTransaction);

        auto result = co_await std::move(recv_task);
        REQUIRE(std::holds_alternative<TestRxMsg>(result));
        CHECK(std::get<TestRxMsg>(result).transportField_transactionId().value() == kTestTransaction);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture,
                 "PodProtocolSession - Variant Receive Throws When No Candidate Matches",
                 "[receive][variant][invalid_id]")
{
    run_test([this]() -> cm::Task<void> {
        auto recv_task = session->async_receive<TestInMsgFake, AnotherFakeMsg>(kTestTransaction, std::chrono::milliseconds(500));
        co_await send_message(TestRxMsg{}, kTestTransaction);
        REQUIRE_THROWS_AS(co_await std::move(recv_task), cm::ProtocolError);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture, "PodProtocolSession - Variant Receive Timeout", "[receive][variant][timeout]")
{
    run_test([this]() -> cm::Task<void> {
        auto recv_task = session->async_receive<TestRxMsg, TestInMsgFake>(kTestTransaction, std::chrono::milliseconds(10));
        REQUIRE_THROWS_AS(co_await std::move(recv_task), cm::TimeoutError);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture,
                 "PodProtocolSession - Single-Type And Variant Overloads Coexist Under The Same Name",
                 "[receive][variant][overload_resolution]")
{
    // Regression test: async_receive<Msg> and async_receive<Msgs...> share a
    // name and must resolve unambiguously based on the number of explicit
    // template arguments. If overload resolution were ambiguous here, this
    // test wouldn't even compile.
    run_test([this]() -> cm::Task<void> {
        auto single_task = session->async_receive<TestRxMsg>(100, std::chrono::milliseconds(500));
        auto variant_task = session->async_receive<TestInMsgFake, TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        co_await send_message(TestRxMsg{}, 100);
        co_await send_message(TestRxMsg{}, kTestTransaction);

        auto single_res = co_await std::move(single_task);
        CHECK(single_res.transportField_transactionId().value() == 100);

        auto variant_res = co_await std::move(variant_task);
        REQUIRE(std::holds_alternative<TestRxMsg>(variant_res));
        CHECK(std::get<TestRxMsg>(variant_res).transportField_transactionId().value() == kTestTransaction);
    });
}

// ---------------------------------------------------------------------------
// One transaction with several replies
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(PodProtocolSessionTestFixture, "PodProtocolSession - Ack And Final Reply In One Read", "[receive][transaction]")
{
    run_test([this]() -> cm::Task<void> {
        auto ack = session->async_receive<cm::InAck, cm::InNak>(kTestTransaction, 500ms);
        auto final_reply = session->async_receive<cm::InDispenseFinished, cm::InNak>(kTestTransaction, 500ms);

        auto bytes = encode_message(cm::InAck{}, kTestTransaction);
        std::ranges::copy(encode_message(cm::InNak{}, kTestTransaction), std::back_inserter(bytes));
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(bytes), exec::asio::use_sender);

        const auto first = co_await std::move(ack);
        CHECK(std::holds_alternative<cm::InAck>(first));
        const auto second = co_await std::move(final_reply);
        CHECK(std::holds_alternative<cm::InNak>(second));
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture, "PodProtocolSession - New Ids Skip Registered Ones", "[transaction]")
{
    run_test([this]() -> cm::Task<void> {
        const auto first = session->generate_new_transaction_id();
        const auto taken = static_cast<cm::TransactionId::ValueType>(first + 1);
        auto pending = session->async_receive<TestRxMsg>(taken, 10ms);

        CHECK(session->generate_new_transaction_id() == static_cast<cm::TransactionId::ValueType>(first + 2));
        REQUIRE_THROWS_AS(co_await std::move(pending), cm::TimeoutError);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture, "PodProtocolSession - A Wrapped Counter Skips Registered Ids", "[transaction]")
{
    run_test([this]() -> cm::Task<void> {
        constexpr auto kLast = std::numeric_limits<cm::TransactionId::ValueType>::max();
        while (session->generate_new_transaction_id() != kLast - 1) {
        }
        auto last = session->async_receive<TestRxMsg>(kLast, 10ms);
        auto zero = session->async_receive<TestRxMsg>(0, 10ms);

        CHECK(session->generate_new_transaction_id() == 1);
        REQUIRE_THROWS_AS(co_await std::move(last), cm::TimeoutError);
        REQUIRE_THROWS_AS(co_await std::move(zero), cm::TimeoutError);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture,
                 "PodProtocolSession - A Wait That Never Starts Releases Its Id",
                 "[transaction][cleanup]")
{
    run_test([this]() -> cm::Task<void> {
        const auto first = session->generate_new_transaction_id();
        const auto next = static_cast<cm::TransactionId::ValueType>(first + 1);
        {
            auto ack = session->async_receive<TestRxMsg>(next, 500ms);
            auto final_reply = session->async_receive<TestRxMsg>(next, 500ms);
            co_await send_message(TestRxMsg{}, next);
            std::ignore = co_await std::move(ack);
        }
        CHECK(session->generate_new_transaction_id() == next);
    });
}

TEST_CASE_METHOD(PodProtocolSessionTestFixture,
                 "PodProtocolSession - Progress Never Pushes The Final Reply Out",
                 "[receive][transaction]")
{
    run_test([this]() -> cm::Task<void> {
        auto ack = session->async_receive<cm::InAck, cm::InNak>(kTestTransaction, 500ms);
        auto final_reply = session->async_receive<cm::InDispenseFinished, cm::InNak>(kTestTransaction, 500ms);

        auto bytes = encode_message(cm::InAck{}, kTestTransaction);
        for (int i = 0; i < 3; ++i) {
            std::ranges::copy(encode_message(cm::InDispenseProgress{}, kTestTransaction), std::back_inserter(bytes));
        }
        std::ranges::copy(encode_message(cm::InDispenseFinished{}, kTestTransaction), std::back_inserter(bytes));
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(bytes), exec::asio::use_sender);

        const auto first = co_await std::move(ack);
        CHECK(std::holds_alternative<cm::InAck>(first));
        const auto second = co_await std::move(final_reply);
        CHECK(std::holds_alternative<cm::InDispenseFinished>(second));
    });
}

namespace {
/// Reads like a socket, but every write fails as if the Pod had gone away.
class FailingWriteStream final : public cm::AnyIoStream
{
  public:
    explicit FailingWriteStream(Socket socket)
        : inner_{std::move(socket)}
    {
    }

    auto async_read(boost::asio::mutable_buffer buffer) -> cm::Task<std::tuple<boost::system::error_code, std::size_t>> override
    {
        return inner_.async_read(buffer);
    }

    auto async_write(boost::asio::const_buffer /*buffer*/, std::chrono::milliseconds /*timeout*/)
        -> cm::Task<boost::system::error_code> override
    {
        co_return boost::asio::error::broken_pipe;
    }

    auto close() -> boost::system::error_code override
    {
        return inner_.close();
    }

    auto get_executor() -> boost::asio::any_io_executor override
    {
        return inner_.get_executor();
    }

  private:
    cm::SocketIoStream<Socket> inner_;
};
} // namespace

TEST_CASE("PodProtocolSession - A failed write ends the session", "[send][lifecycle]")
{
    boost::asio::io_context ioc;
    Socket peer{ioc};
    Socket session_socket{ioc};
    boost::asio::local::connect_pair(peer, session_socket);
    Session session{std::make_unique<FailingWriteStream>(std::move(session_socket))};

    bool ended = false;
    auto send_and_wait = [&]() -> cm::Task<void> {
        co_await session.async_send(TestTxMsg{}, kTestTransaction);
        co_await cm::async_sleep(ioc.get_executor(), 2s);
    };
    cm::run_until_complete(ioc, exec::when_any(session.run() | stdexec::then([&] { ended = true; }), send_and_wait()));

    CHECK(ended);
}
