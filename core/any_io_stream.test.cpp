#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <catch2/catch_test_macros.hpp>
#include <exec/when_any.hpp>
#include <stdexec/execution.hpp>
import std;
import cm.core;
import cm.testing;

using namespace std::chrono_literals;

namespace {
using Socket = boost::asio::local::stream_protocol::socket;

/// A stream on one end of a socket pair, with the other end left to the test.
struct StreamFixture
{
    boost::asio::io_context ioc;
    Socket peer{ioc};
    std::unique_ptr<cm::SocketIoStream<Socket>> stream;

    StreamFixture()
    {
        Socket local{ioc};
        boost::asio::local::connect_pair(peer, local);
        stream = std::make_unique<cm::SocketIoStream<Socket>>(std::move(local));
    }

    boost::system::error_code write(boost::asio::const_buffer buffer, std::chrono::milliseconds timeout)
    {
        boost::system::error_code ec;
        REQUIRE(cm::run_until_complete(ioc, [&]() -> cm::Task<void> { ec = co_await stream->async_write(buffer, timeout); }()));
        return ec;
    }
};
} // namespace

TEST_CASE_METHOD(StreamFixture, "SocketIoStream - A write nobody reads times out", "[any_io_stream]")
{
    // Far more than the socket buffer holds, so the write has to wait for a reader that never comes.
    const std::vector<std::uint8_t> data(64 * 1024 * 1024);

    CHECK(write(boost::asio::buffer(data), 20ms) == boost::asio::error::timed_out);
}

TEST_CASE_METHOD(StreamFixture, "SocketIoStream - A write to a closed peer reports the error", "[any_io_stream]")
{
    const std::array<std::uint8_t, 4> data{1, 2, 3, 4};
    peer.close();

    const auto ec = write(boost::asio::buffer(data), 500ms);

    CHECK(ec);
    CHECK(ec != boost::asio::error::timed_out);
}

TEST_CASE_METHOD(StreamFixture, "SocketIoStream - A write that gets through reports no error", "[any_io_stream]")
{
    const std::array<std::uint8_t, 4> data{1, 2, 3, 4};

    CHECK_FALSE(write(boost::asio::buffer(data), 500ms));
}

TEST_CASE_METHOD(StreamFixture, "SocketIoStream - A stop request ends a pending read as stopped", "[any_io_stream]")
{
    std::array<std::uint8_t, 16> buffer{};
    bool read_returned = false;
    bool read_stopped = false;

    REQUIRE(cm::run_until_complete(ioc,
                                   exec::when_any(stream->async_read(boost::asio::buffer(buffer)) | stdexec::then([&](auto&&) {
                                                      read_returned = true;
                                                  }) | stdexec::upon_stopped([&]() noexcept { read_stopped = true; }),
                                                  cm::async_sleep(ioc.get_executor(), 10ms))));

    CHECK(read_stopped);
    CHECK_FALSE(read_returned);
}
