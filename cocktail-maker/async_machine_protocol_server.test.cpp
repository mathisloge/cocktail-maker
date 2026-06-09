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

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Successful Receive", "[receive][success]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        TestRxMsg out_msg;
        TestClientOutFrame frame;

        out_msg.transportField_transactionId().setValue(kTestTransaction);

        std::vector<uint8_t> buffer(frame.length(out_msg));
        auto* iter = buffer.data();
        auto write_es = frame.write(out_msg, iter, buffer.size());
        REQUIRE(write_es == comms::ErrorStatus::Success);

        // 1. Invoke async_receive first. This synchronously registers the channel in dispatch_map_
        //    before suspending. We hold onto the returned promise/task.
        auto recv_task = server->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        // 2. Safely blast the data over the socket.
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), boost::cobalt::use_op);

        // 3. Await the outcome. The background read_loop will route the incoming frame to recv_task.
        auto res = co_await recv_task;
        CHECK(res.getId() == TestRxMsg::staticMsgId());
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Receive Fragmented Message", "[receive][fragmented]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        TestRxMsg out_msg;
        out_msg.transportField_transactionId().setValue(kTestTransaction);
        TestClientOutFrame frame;
        std::vector<uint8_t> buffer(frame.length(out_msg));
        auto* iter = buffer.data();
        frame.write(out_msg, iter, buffer.size());

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

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Receive Invalid Msg ID", "[receive][invalid_id]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        TestRxMsg out_msg;
        out_msg.transportField_transactionId().setValue(kTestTransaction);
        TestClientOutFrame frame;
        std::vector<uint8_t> buffer(frame.length(out_msg));
        auto* iter = buffer.data();
        frame.write(out_msg, iter, buffer.size());

        auto recv_task = server->async_receive<TestInMsgFake>(kTestTransaction, std::chrono::milliseconds(500));
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), boost::cobalt::use_op);
        REQUIRE_THROWS_AS(co_await recv_task, cm::ProtocolError);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Dispatcher Drops Unmatched Transaction IDs",
                 "[receive][dispatch]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        TestRxMsg out_msg;
        out_msg.transportField_transactionId().setValue(kTestTransaction);
        TestClientOutFrame frame;
        std::vector<uint8_t> buffer(frame.length(out_msg));
        auto* iter = buffer.data();
        frame.write(out_msg, iter, buffer.size());

        auto recv_task1 = server->async_receive<TestRxMsg>(100, std::chrono::milliseconds(500));
        auto recv_task2 = server->async_receive<TestRxMsg>(kTestTransaction, std::chrono::milliseconds(500));

        // Blast identical messages over the socket
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), boost::cobalt::use_op);

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
                 "AsyncMachineProtocolServer - Receive Multiple Events via Generator",
                 "[receive_events][success]")
{
    run_test([this]() -> boost::cobalt::task<void> {
        TestRxMsg out_msg;
        TestClientOutFrame frame;
        std::vector<uint8_t> buffer;

        // Construct a buffer containing 3 consecutive messages
        for (int i = 0; i < 3; ++i) {
            size_t start = buffer.size();
            buffer.resize(start + frame.length(out_msg));
            auto* iter = buffer.data() + start;
            REQUIRE(frame.write(out_msg, iter, buffer.size() - start) == comms::ErrorStatus::Success);
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
