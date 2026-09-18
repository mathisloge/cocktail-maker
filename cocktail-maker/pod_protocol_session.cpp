module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/system_error.hpp>
#include <comms/ErrorStatus.h>
#include <exec/asio/use_sender.hpp>
#include <exec/when_any.hpp>
#include <libassert/assert-macros.hpp>
#include <proto/MsgId.h>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

module cm:pod_protocol_session_impl;
import std;
import libassert;
import cm.core;
import :pod_protocol_session;

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
// PodProtocolSession
// ---------------------------------------------------------------------------

PodProtocolSession::~PodProtocolSession()
{
    try {
        ASSERT(!is_running_, "Object destroyed while asynchronous loops were still running!");
        // Force close stream and channels synchronously to prevent hanging the io_context
        std::ignore = stream_->close();
        shutdown_channels();
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_ERROR(logger_, "Error while destructing the session: {}", ex.what());
    }
}

PodProtocolSession::PodProtocolSession(std::unique_ptr<AnyIoStream> stream)
    : logger_{log::create_or_get("protocol")}
    , stream_{std::move(stream)}
    , write_queue_{stream_->get_executor(), kWriteQueueCapacity}
{
}

auto PodProtocolSession::get_executor() -> asio::any_io_executor
{
    return stream_->get_executor();
}

Task<void> PodProtocolSession::run()
{
    // A stop request unwinds the coroutine at the co_await, so the cleanup has to run in a destructor.
    struct RunningGuard
    {
        PodProtocolSession& session;

        explicit RunningGuard(PodProtocolSession& s)
            : session{s}
        {
            session.is_running_ = true;
        }

        RunningGuard(const RunningGuard&) = delete;
        RunningGuard& operator=(const RunningGuard&) = delete;

        ~RunningGuard()
        {
            session.is_running_ = false;
            session.shutdown_channels();
        }
    } running_guard{*this};

    try {
        co_await exec::when_any(read_loop(), write_loop());
    }
    catch (const boost::system::system_error& e) {
        SPDLOG_LOGGER_ERROR(logger_, "I/O loops terminated: {}", e.what());
    }
}

TransactionId::ValueType PodProtocolSession::generate_new_transaction_id()
{
    return ++transaction_id_counter_;
}

auto PodProtocolSession::read_with_timeout(ResponseChannel& chan, std::chrono::milliseconds timeout) -> Task<InFrame::MsgPtr>
{
    using Response = std::optional<InFrame::MsgPtr>;
    asio::steady_timer timer{stream_->get_executor(), timeout};

    // Race the channel read against the timer. The loser is stopped.
    auto response = co_await exec::when_any(cm::async_receive(chan) |
                                                stdexec::then([](InFrame::MsgPtr msg) { return Response{std::move(msg)}; }),
                                            timer.async_wait(exec::asio::use_sender) | stdexec::then([] { return Response{}; }));

    // An empty response means that the timer fired first.
    if (!response.has_value()) {
        throw TimeoutError{"Could not receive any message"};
    }

    co_return std::move(*response);
}

auto PodProtocolSession::register_response(TransactionId::ValueType id) -> ResponseRegistration
{
    auto it = dispatch_map_.find(id);
    if (it == dispatch_map_.end()) {
        it =
            dispatch_map_.emplace(id, std::make_shared<ResponseChannel>(stream_->get_executor(), kResponseChannelCapacity)).first;
    }
    return ResponseRegistration{*this, id, it->second};
}

void PodProtocolSession::shutdown_channels()
{
    write_queue_.close();
    for (auto&& [id, chan] : dispatch_map_) {
        chan->close();
    }
    dispatch_map_.clear();
}

Task<void> PodProtocolSession::write_loop()
{
    while (true) {
        std::vector<uint8_t> data;
        try {
            data = co_await cm::async_receive(write_queue_);
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

Task<void> PodProtocolSession::read_loop()
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
                // Every response channel has room for exactly the one response it is registered for.
                if (!chan->try_send(boost::system::error_code{}, std::move(msg))) {
                    SPDLOG_LOGGER_WARN(logger_, "Dispatch to channel of transaction id '{}' failed.", transaction_id);
                }
            }
            else {
                SPDLOG_LOGGER_WARN(logger_, "Discarding unhandled message '{}'.", msg->name());
            }

            consume_front(consumed);
        }
    }
}

PodProtocolSession::ResponseRegistration::ResponseRegistration(PodProtocolSession& session,
                                                               TransactionId::ValueType transaction_id,
                                                               ChannelPtr channel)
    : session_{&session}
    , transaction_id_{transaction_id}
    , channel_{std::move(channel)}
{
}

PodProtocolSession::ResponseRegistration::ResponseRegistration(ResponseRegistration&& other) noexcept
    : session_{std::exchange(other.session_, nullptr)}
    , transaction_id_{other.transaction_id_}
    , channel_{std::move(other.channel_)}
{
}

PodProtocolSession::ResponseRegistration::~ResponseRegistration()
{
    if (session_ == nullptr) {
        return;
    }
    // The entry may already have been dispatched and replaced by a new registration for the same transaction id.
    const auto it = session_->dispatch_map_.find(transaction_id_);
    if (it != session_->dispatch_map_.end() && it->second == channel_) {
        session_->dispatch_map_.erase(it);
    }
}

auto PodProtocolSession::ResponseRegistration::channel() const -> ResponseChannel&
{
    return *channel_;
}

} // namespace cm
