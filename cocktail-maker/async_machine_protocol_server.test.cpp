#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <catch2/catch_test_macros.hpp>
#include <comms/ErrorStatus.h>

import std;
import cm;

using Socket = boost::asio::local::stream_protocol::socket;
using Server = cm::AsyncMachineProtocolServer<Socket>;

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

struct AsyncServerTestFixture
{
    boost::asio::io_context ioc;
    Socket client_socket;
    std::unique_ptr<Server> server;

    AsyncServerTestFixture()
        : client_socket(ioc)
    {
        boost::cobalt::this_thread::set_executor(ioc.get_executor());
        Socket server_socket{ioc};
        boost::asio::local::connect_pair(client_socket, server_socket);
        server = std::make_unique<Server>(std::move(server_socket));
    }

    // Safely executes tests and guarantees graceful server shutdown even if assertions fail
    template <typename CoroutineFunc>
    void run_test(CoroutineFunc&& test_coro)
    {
        std::exception_ptr err;

        auto test_wrapper = [&]() -> boost::cobalt::task<void> {
            try {
                co_await boost::cobalt::race(test_coro(), server->run());
            }
            catch (...) {
                // Catch2 REQUIRE failures throw an exception to abort the execution path
                err = std::current_exception();
            }
        };

        boost::cobalt::spawn(ioc, test_wrapper(), boost::asio::detached);
        ioc.run();

        if (err) {
            std::rethrow_exception(err);
        }
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
    boost::cobalt::task<void> send_message(Msg msg, cm::TransactionId::ValueType transaction_id)
    {
        auto buffer = encode_message(std::move(msg), transaction_id);
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), boost::cobalt::use_op);
    }
};

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Lifecycle Starts and Stops Cleanly", "[lifecycle]")
{
    run_test([&]() -> boost::cobalt::task<void> {
        CHECK(true);
        co_return;
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Sends Messages over Socket", "[send]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        TestTxMsg msg;
        REQUIRE_NOTHROW(co_await server->async_send(msg, kTestTransaction));

        std::vector<uint8_t> buffer(1024);
        auto [ec, bytes_read] =
            co_await client_socket.async_read_some(boost::asio::buffer(buffer), boost::asio::as_tuple(boost::cobalt::use_op));

        REQUIRE(!ec);
        CHECK(bytes_read > 0);

        CHECK(static_cast<int>(buffer[6]) == kTestTransaction);
    });
}

// ---------------------------------------------------------------------------
// Single-type async_receive<Msg> -> Msg
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Successful Receive", "[receive][success]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        // 1. Invoke async_receive first. This synchronously registers the channel in dispatch_map_
        //    before suspending. We hold onto the returned promise/task.
        auto recv_task = server->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        // 2. Safely blast the data over the socket.
        co_await send_message(TestRxMsg{}, kTestTransaction);

        // 3. Await the outcome. The background read_loop will route the incoming frame to recv_task.
        auto res = co_await recv_task;
        CHECK(res.getId() == TestRxMsg::staticMsgId());
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Receive Fragmented Message", "[receive][fragmented]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        auto buffer = encode_message(TestRxMsg{}, kTestTransaction);
        REQUIRE(buffer.size() > 1);
        size_t half = buffer.size() / 2;

        auto recv_task = server->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        // Write first half
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer.data(), half), boost::cobalt::use_op);

        // Introduce a slight delay before writing the second half
        boost::asio::steady_timer delay_timer(ioc, std::chrono::milliseconds(10));
        co_await delay_timer.async_wait(boost::cobalt::use_op);

        co_await boost::asio::async_write(
            client_socket, boost::asio::buffer(buffer.data() + half, buffer.size() - half), boost::cobalt::use_op);

        auto res = co_await recv_task;
        REQUIRE(res.transportField_transactionId().getDisplayValue() == kTestTransaction);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Receive Message Fragmented Across Three Writes",
                 "[receive][fragmented]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        auto buffer = encode_message(TestRxMsg{}, kTestTransaction);
        REQUIRE(buffer.size() >= 3);
        size_t third = buffer.size() / 3;

        auto recv_task = server->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        boost::asio::steady_timer delay_timer(ioc);

        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer.data(), third), boost::cobalt::use_op);

        delay_timer.expires_after(std::chrono::milliseconds(5));
        co_await delay_timer.async_wait(boost::cobalt::use_op);

        co_await boost::asio::async_write(
            client_socket, boost::asio::buffer(buffer.data() + third, third), boost::cobalt::use_op);

        delay_timer.expires_after(std::chrono::milliseconds(5));
        co_await delay_timer.async_wait(boost::cobalt::use_op);

        co_await boost::asio::async_write(
            client_socket, boost::asio::buffer(buffer.data() + 2 * third, buffer.size() - 2 * third), boost::cobalt::use_op);

        auto res = co_await recv_task;
        CHECK(res.transportField_transactionId().getDisplayValue() == kTestTransaction);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Receive Invalid Msg ID", "[receive][invalid_id]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        auto recv_task = server->async_receive<TestInMsgFake>(kTestTransaction, std::chrono::milliseconds(500));
        co_await send_message(TestRxMsg{}, kTestTransaction);
        REQUIRE_THROWS_AS(co_await recv_task, cm::ProtocolError);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Dispatcher Drops Unmatched Transaction IDs",
                 "[receive][dispatch]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        auto recv_task1 = server->async_receive<TestRxMsg>(100, std::chrono::milliseconds(500));
        auto recv_task2 = server->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        // Blast identical messages over the socket
        co_await send_message(TestRxMsg{}, kTestTransaction);

        REQUIRE_THROWS_AS(co_await recv_task1, cm::TimeoutError);
        auto res2 = co_await recv_task2;
        CHECK(res2.transportField_transactionId().value() == kTestTransaction);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Receive Timeouts", "[receive][timeout]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        REQUIRE_THROWS_AS(co_await server->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(10)),
                          cm::TimeoutError);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Channel Cleanup After Timeout Allows Reuse Of Transaction Id",
                 "[receive][cleanup]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        // CleanupGuard must erase the dispatch_map_ entry on the timeout exit
        // path, otherwise this transaction id would stay "stuck" forever.
        REQUIRE_THROWS_AS(co_await server->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(10)),
                          cm::TimeoutError);

        auto recv_task = server->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));
        co_await send_message(TestRxMsg{}, kTestTransaction);
        auto res = co_await recv_task;
        CHECK(res.transportField_transactionId().value() == kTestTransaction);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Channel Cleanup After Protocol Error Allows Reuse Of Transaction Id",
                 "[receive][cleanup]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        // Same as above, but exercising the exception exit path (id mismatch)
        // rather than the timeout exit path.
        auto bad_recv = server->async_receive<TestInMsgFake>(kTestTransaction, std::chrono::milliseconds(500));
        co_await send_message(TestRxMsg{}, kTestTransaction);
        REQUIRE_THROWS_AS(co_await bad_recv, cm::ProtocolError);

        auto good_recv = server->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));
        co_await send_message(TestRxMsg{}, kTestTransaction);
        auto res = co_await good_recv;
        CHECK(res.transportField_transactionId().value() == kTestTransaction);
    });
}

// ---------------------------------------------------------------------------
// Variant async_receive<Msgs...> -> std::variant<Msgs...>
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Variant Receive Matches A Later Candidate In The Pack",
                 "[receive][variant]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        // TestInMsgFake never matches the wire id; this exercises the fold
        // expression correctly skipping a mismatched candidate before
        // landing on the one that actually matches.
        auto recv_task = server->async_receive<TestInMsgFake, TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        co_await send_message(TestRxMsg{}, kTestTransaction);

        auto result = co_await recv_task;
        REQUIRE(std::holds_alternative<TestRxMsg>(result));
        CHECK(std::get<TestRxMsg>(result).transportField_transactionId().value() == kTestTransaction);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Variant Receive Throws When No Candidate Matches",
                 "[receive][variant][invalid_id]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        auto recv_task = server->async_receive<TestInMsgFake, AnotherFakeMsg>(kTestTransaction, std::chrono::milliseconds(500));
        co_await send_message(TestRxMsg{}, kTestTransaction);
        REQUIRE_THROWS_AS(co_await recv_task, cm::ProtocolError);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Variant Receive Timeout", "[receive][variant][timeout]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        auto recv_task = server->async_receive<TestRxMsg, TestInMsgFake>(kTestTransaction, std::chrono::milliseconds(10));
        REQUIRE_THROWS_AS(co_await recv_task, cm::TimeoutError);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Single-Type And Variant Overloads Coexist Under The Same Name",
                 "[receive][variant][overload_resolution]")
{
    // Regression test: async_receive<Msg> and async_receive<Msgs...> share a
    // name and must resolve unambiguously based on the number of explicit
    // template arguments. If overload resolution were ambiguous here, this
    // test wouldn't even compile.
    run_test([this]() -> boost::cobalt::task<void> {
        auto single_task = server->async_receive<TestRxMsg>(100, std::chrono::milliseconds(500));
        auto variant_task = server->async_receive<TestInMsgFake, TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        co_await send_message(TestRxMsg{}, 100);
        co_await send_message(TestRxMsg{}, kTestTransaction);

        auto single_res = co_await single_task;
        CHECK(single_res.transportField_transactionId().value() == 100);

        auto variant_res = co_await variant_task;
        REQUIRE(std::holds_alternative<TestRxMsg>(variant_res));
        CHECK(std::get<TestRxMsg>(variant_res).transportField_transactionId().value() == kTestTransaction);
    });
}

// ---------------------------------------------------------------------------
// async_receive_events<Msg> (unsolicited event generator)
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Receive Multiple Events via Generator",
                 "[receive_events][success]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        // Construct a buffer containing 3 consecutive messages
        std::vector<uint8_t> buffer;
        for (int i = 0; i < 3; ++i) {
            auto frame_bytes = encode_message(TestRxMsg{}, kTestTransaction);
            buffer.insert(buffer.end(), frame_bytes.begin(), frame_bytes.end());
        }

        // Create the event subscriber (Generator)
        auto events = server->async_receive_events<TestRxMsg>();

        // Blast all 3 messages over the socket
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), boost::cobalt::use_op);

        // Await them in a loop
        for (int i = 0; i < 3; ++i) {
            auto ev = co_await events;
            REQUIRE(std::holds_alternative<TestRxMsg>(ev));
        }

        CHECK(true);
    });
}
