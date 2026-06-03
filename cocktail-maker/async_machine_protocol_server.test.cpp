#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <catch2/catch_test_macros.hpp>
#include <comms/ErrorStatus.h>

import std;
import cm;

using Socket = boost::asio::local::stream_protocol::socket;
using Server = cm::AsyncMachineProtocolServer<Socket>;

using TestInMsg2 = cm::InMsg1;
using TestClientOutFrame = cm::OutFrame;
using TestClientOutMsg2 = cm::Msg2;

struct AsyncServerTestFixture
{
    boost::asio::io_context ioc;
    Socket client_socket;
    Socket server_socket;

    AsyncServerTestFixture()
        : client_socket(ioc)
        , server_socket(ioc)
    {
        boost::cobalt::this_thread::set_executor(ioc.get_executor());
        boost::asio::local::connect_pair(client_socket, server_socket);
    }

    // Safely executes tests and guarantees graceful server shutdown even if assertions fail
    template <typename CoroutineFunc>
    void run_test(Server& server, CoroutineFunc&& test_coro)
    {
        std::exception_ptr err;

        auto test_wrapper = [&]() -> boost::cobalt::task<void> {
            try {
                co_await test_coro();
            }
            catch (...) {
                // Catch2 REQUIRE failures throw an exception to abort the execution path
                err = std::current_exception();
            }

            // ALWAYS guarantee server is stopped so ioc.run() can safely finish.
            co_await server.stop();
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
    Server server(std::move(server_socket));
    boost::cobalt::spawn(ioc, server.run(), boost::asio::detached);

    run_test(server, [&]() -> boost::cobalt::task<void> {
        CHECK(true);
        co_return;
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Sends Messages over Socket", "[send]")
{
    Server server(std::move(server_socket));
    boost::cobalt::spawn(ioc, server.run(), boost::asio::detached);

    run_test(server, [&]() -> boost::cobalt::task<void> {
        cm::Msg2 msg;
        auto send_res = co_await server.async_send(msg);
        REQUIRE(send_res.has_value());

        std::vector<uint8_t> buffer(1024);
        auto [ec, bytes_read] =
            co_await client_socket.async_read_some(boost::asio::buffer(buffer), boost::asio::as_tuple(boost::cobalt::use_op));

        REQUIRE(!ec);
        CHECK(bytes_read > 0);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Successful Receive", "[receive][success]")
{
    Server server(std::move(server_socket));
    boost::cobalt::spawn(ioc, server.run(), boost::asio::detached);

    run_test(server, [&]() -> boost::cobalt::task<void> {
        TestInMsg2 out_msg;
        TestClientOutFrame frame;

        std::vector<uint8_t> buffer(frame.length(out_msg));
        auto* iter = buffer.data();
        auto write_es = frame.write(out_msg, iter, buffer.size());
        REQUIRE(write_es == comms::ErrorStatus::Success);

        // 1. Invoke async_receive first. This synchronously registers the channel in dispatch_map_
        //    before suspending. We hold onto the returned promise/task.
        auto recv_task = server.async_receive<TestInMsg2>(std::chrono::milliseconds(500));

        // 2. Safely blast the data over the socket.
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), boost::cobalt::use_op);

        // 3. Await the outcome. The background read_loop will route the incoming frame to recv_task.
        auto res = co_await recv_task;

        if (!res.has_value()) {
            FAIL("async_receive failed. comms::ErrorStatus: " << static_cast<int>(res.error()));
        }
        REQUIRE(res.has_value());
        CHECK(res.value().getId() == TestInMsg2::staticMsgId());
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Receive Fragmented Message", "[receive][fragmented]")
{
    Server server(std::move(server_socket));
    boost::cobalt::spawn(ioc, server.run(), boost::asio::detached);

    run_test(server, [&]() -> boost::cobalt::task<void> {
        TestInMsg2 out_msg;
        TestClientOutFrame frame;
        std::vector<uint8_t> buffer(frame.length(out_msg));
        auto* iter = buffer.data();
        frame.write(out_msg, iter, buffer.size());

        REQUIRE(buffer.size() > 1);
        size_t half = buffer.size() / 2;

        auto recv_task = server.async_receive<TestInMsg2>(std::chrono::milliseconds(500));

        // Write first half (simulating TCP fragmentation)
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer.data(), half), boost::cobalt::use_op);

        // Introduce a slight delay before writing the second half
        boost::asio::steady_timer delay_timer(ioc, std::chrono::milliseconds(10));
        co_await delay_timer.async_wait(boost::cobalt::use_op);

        co_await boost::asio::async_write(
            client_socket, boost::asio::buffer(buffer.data() + half, buffer.size() - half), boost::cobalt::use_op);

        auto res = co_await recv_task;
        if (!res.has_value()) {
            FAIL("async_receive failed on fragmented read. comms::ErrorStatus: " << static_cast<int>(res.error()));
        }
        REQUIRE(res.has_value());
    });
}

struct TestInMsgFake : public TestInMsg2
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
    Server server(std::move(server_socket));
    boost::cobalt::spawn(ioc, server.run(), boost::asio::detached);

    run_test(server, [&]() -> boost::cobalt::task<void> {
        TestInMsg2 out_msg;
        TestClientOutFrame frame;
        std::vector<uint8_t> buffer(frame.length(out_msg));
        auto* iter = buffer.data();
        frame.write(out_msg, iter, buffer.size());

        auto recv_task = server.async_receive<TestInMsgFake>(std::chrono::milliseconds(500));
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), boost::cobalt::use_op);
        auto res = co_await recv_task;

        REQUIRE_FALSE(res.has_value());
        CHECK(res.error() == comms::ErrorStatus::InvalidMsgId);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Dispatcher Drops Unmatched Transaction IDs",
                 "[receive][dispatch]")
{
    Server server(std::move(server_socket));
    boost::cobalt::spawn(ioc, server.run(), boost::asio::detached);

    run_test(server, [&]() -> boost::cobalt::task<void> {
        TestInMsg2 out_msg;
        TestClientOutFrame frame;
        std::vector<uint8_t> buffer(frame.length(out_msg));
        auto* iter = buffer.data();
        frame.write(out_msg, iter, buffer.size());

        auto recv_task = server.async_receive<TestInMsg2>(std::chrono::milliseconds(500));

        // Blast identical messages over the socket
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), boost::cobalt::use_op);
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), boost::cobalt::use_op);

        auto res1 = co_await recv_task;
        REQUIRE(res1.has_value());

        // Second duplicate message is orphaned and safely dropped in background.
        // A new receive requests TransactionId 1. It will time out because we aren't sending one.
        auto res2 = co_await server.async_receive<TestInMsg2>(std::chrono::milliseconds(50));
        REQUIRE_FALSE(res2.has_value());
        CHECK(res2.error() == comms::ErrorStatus::ProtocolError);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture, "AsyncMachineProtocolServer - Receive Timeouts", "[receive][timeout]")
{
    Server server(std::move(server_socket));
    boost::cobalt::spawn(ioc, server.run(), boost::asio::detached);

    run_test(server, [&]() -> boost::cobalt::task<void> {
        auto res = co_await server.async_receive<TestInMsg2>(std::chrono::milliseconds(10));
        REQUIRE_FALSE(res.has_value());
        CHECK(res.error() == comms::ErrorStatus::ProtocolError);
    });
}

TEST_CASE_METHOD(AsyncServerTestFixture,
                 "AsyncMachineProtocolServer - Receive Multiple Events via Generator",
                 "[receive_events][success]")
{
    Server server(std::move(server_socket));
    boost::cobalt::spawn(ioc, server.run(), boost::asio::detached);

    run_test(server, [&]() -> boost::cobalt::task<void> {
        TestInMsg2 out_msg;
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
        auto events = server.async_receive_events<TestInMsg2>();

        // Blast all 3 messages over the socket
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(buffer), boost::cobalt::use_op);

        // Await them in a loop
        for (int i = 0; i < 3; ++i) {
            auto ev = co_await events;
            REQUIRE(std::holds_alternative<TestInMsg2>(ev));
        }

        CHECK(true);
    });
}
