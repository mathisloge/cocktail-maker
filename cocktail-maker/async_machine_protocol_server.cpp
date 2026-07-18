module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt/channel.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/race.hpp>
#include <boost/cobalt/task.hpp>
#include <comms/ErrorStatus.h>
#include <libassert/assert-macros.hpp>
#include <proto/MsgId.h>
#include <spdlog/spdlog.h>

module cm:async_machine_protocol_server_impl;
import std;
import libassert;
import :async_machine_protocol_server;

namespace cobalt = boost::cobalt;
namespace asio = boost::asio;

namespace cm {

// ---------------------------------------------------------------------------
// ProtocolError
// ---------------------------------------------------------------------------

ProtocolError::ProtocolError(comms::ErrorStatus error_status, std::string human_readable_what)
    : std::runtime_error{std::format("{} ErrorCode: {}", std::move(human_readable_what), error_status)}
    , error_status_{error_status}
{
}

comms::ErrorStatus ProtocolError::error_status() const
{
    return error_status_;
}

// ---------------------------------------------------------------------------
// AsyncMachineProtocolServer
// ---------------------------------------------------------------------------

AsyncMachineProtocolServer::~AsyncMachineProtocolServer()
{
    try {
        ASSERT(!is_running_, "Object destroyed while asynchronous loops were still running!");
        // Force close stream and channels synchronously to prevent hanging the io_context
        std::ignore = stream_->close();
        shutdown_channels();
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_ERROR(logger_, "Error while descructing the server: {}", ex.what());
    }
}

AsyncMachineProtocolServer::AsyncMachineProtocolServer(std::unique_ptr<AnyIoStream> stream)
    : logger_{log::create_or_get("protocol")}
    , stream_{std::move(stream)}
    , write_queue_{kWriteQueueCapacity, stream_->get_executor()}
{
}

auto AsyncMachineProtocolServer::get_executor() -> asio::any_io_executor
{
    return stream_->get_executor();
}

cobalt::task<void> AsyncMachineProtocolServer::run()
{
    is_running_ = true;
    try {
        co_await boost::cobalt::race(read_loop(), write_loop());
    }
    catch (const boost::system::system_error& e) {
        SPDLOG_LOGGER_ERROR(logger_, "I/O loops terminated: {}", e.what());
    }
    is_running_ = false;
    shutdown_channels();
}

TransactionId::ValueType AsyncMachineProtocolServer::generate_new_transaction_id()
{
    return ++transaction_id_counter_;
}

auto AsyncMachineProtocolServer::read_with_timeout(cobalt::channel<InFrame::MsgPtr>& chan, std::chrono::milliseconds timeout)
    -> cobalt::promise<InFrame::MsgPtr>
{
    boost::asio::steady_timer timer{stream_->get_executor()};
    timer.expires_after(timeout);

    // Race the channel read against the timer
    auto res = co_await cobalt::race(chan.read(), timer.async_wait(cobalt::use_op));

    // If index 1 wins, the timer fired first (Timeout)
    if (res.index() == 1) {
        throw TimeoutError{"Could not receive any message"};
    }

    co_return std::move(boost::variant2::get<0>(res));
}

auto AsyncMachineProtocolServer::get_or_create_channel(TransactionId::ValueType id) -> ChannelPtr
{
    auto it = dispatch_map_.find(id);
    if (it == dispatch_map_.end()) {
        it = dispatch_map_.emplace(id, std::make_shared<cobalt::channel<InFrame::MsgPtr>>(kResponseChannelCapacity)).first;
    }
    return it->second;
}

void AsyncMachineProtocolServer::shutdown_channels()
{
    write_queue_.close();
    for (auto&& [id, chan] : dispatch_map_) {
        chan->close();
    }
    dispatch_map_.clear();
}

cobalt::task<void> AsyncMachineProtocolServer::write_loop()
{
    while (true) {
        std::vector<uint8_t> data;
        try {
            data = co_await write_queue_.read();
        }
        catch (...) {
            break; // Queue closed due to shutdown_channels()
        }

        try {
            co_await stream_->async_write(asio::buffer(data), std::chrono::milliseconds(500));
        }
        catch (const boost::system::system_error& e) {
            SPDLOG_LOGGER_ERROR(logger_, "Write aborted: {}", e.what());
            break;
        }
    }
}

cobalt::task<void> AsyncMachineProtocolServer::read_loop()
{
    static constexpr std::size_t kMaxBufferSize = 64 * 1024;

    std::vector<std::uint8_t> rx_buffer(4096);
    std::size_t valid_bytes = 0;

    auto consume_front = [&](std::size_t count) {
        if (count == 0) {
            return;
        }

        if (count >= valid_bytes) {
            valid_bytes = 0;
            return;
        }

        const std::size_t remaining = valid_bytes - count;

        std::move(rx_buffer.begin() + count, rx_buffer.begin() + valid_bytes, rx_buffer.begin());
        valid_bytes = remaining;
    };

    InFrame frame;

    while (true) {
        if (valid_bytes == rx_buffer.size()) {
            if (rx_buffer.size() >= kMaxBufferSize) {
                SPDLOG_LOGGER_ERROR(logger_, "Fatal: Buffer limit exceeded. Dropping connection to prevent OOM.");
                co_return;
            }
            rx_buffer.resize(rx_buffer.size() * 2);
        }

        auto [ec, bytes_read] =
            co_await stream_->async_read(asio::buffer(rx_buffer.data() + valid_bytes, rx_buffer.size() - valid_bytes));

        if (ec) {
            SPDLOG_LOGGER_ERROR(logger_, "Could not read from stream. Returning from read-loop. Reason: {}", ec.message());
            co_return;
        }

        valid_bytes += bytes_read;

        for (;;) {
            InFrame::MsgPtr msg;
            const std::uint8_t* iter = rx_buffer.data();

            const auto es = frame.read(msg, iter, valid_bytes);
            const std::size_t consumed = static_cast<std::size_t>(iter - rx_buffer.data());

            if (es == comms::ErrorStatus::NotEnoughData) {
                break; // Keep buffered bytes, wait for more data
            }

            if (es != comms::ErrorStatus::Success) {
                consume_front((consumed != 0U) ? consumed : 1U);
                continue;
            }

            if (msg == nullptr) {
                consume_front((consumed != 0U) ? consumed : 1U);
                continue;
            }
            const auto transaction_id = std::get<TransactionId>(msg->transportFields()).value();

            SPDLOG_LOGGER_TRACE(logger_, "Received message '{}' with transaction id '{}'", msg->name(), transaction_id);

            const auto rx_it = dispatch_map_.find(transaction_id);
            if (rx_it != dispatch_map_.end()) {
                auto chan = rx_it->second;
                dispatch_map_.erase(rx_it);
                try {
                    co_await chan->write(std::move(msg));
                }
                catch (const boost::system::system_error& write_error) {
                    SPDLOG_LOGGER_WARN(logger_,
                                       "Dispatch to channel of transaction id '{}' failed with: {}",
                                       transaction_id,
                                       write_error.what());
                }
            }
            else {
                SPDLOG_LOGGER_WARN(logger_, "Discarding unhandled message '{}'.", msg->name());
            }

            consume_front(consumed);
        }
    }
}

AsyncMachineProtocolServer::CleanupGuard::~CleanupGuard()
{
    client->dispatch_map_.erase(transaction_id);
}

} // namespace cm
