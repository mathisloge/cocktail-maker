module;
#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <comms/GenericMessage.h>
#include <comms/dispatch.h>
#include <comms/options.h>
#include <cstddef>
#include <proto/FrameInterface.h>
#include <proto/dispatch/DispatchServerInputMessage.h>
#include <proto/frame/Frame.h>
#include <proto/input/AllMessages.h>
#include <proto/input/ServerInputMessages.h>
#include <proto/options/ServerDefaultOptions.h>

export module cm:async_machine_protocol_server;

import std;
import :logging;

namespace cm {

namespace cobalt = boost::cobalt;
namespace asio = boost::asio;

using ServerOptions = proto::options::ServerDefaultOptions;
using InMessage = proto::FrameInterface<comms::option::app::ReadIterator<const std::uint8_t*>,
                                        comms::option::app::LengthInfoInterface,
                                        comms::option::app::IdInfoInterface,
                                        comms::option::app::NameInterface>;

using InFrame = proto::frame::Frame<InMessage, proto::input::ServerInputMessages<InMessage>, ServerOptions>;

// All server input messages
export using InPong = proto::message::Pong<InMessage>;
export using InAck = proto::message::Ack<InMessage>;
export using InDeviceInfoResponse = proto::message::DeviceInfoResponse<InMessage>;

export using OutMessage = proto::FrameInterface<comms::option::app::WriteIterator<std::uint8_t*>,
                                                comms::option::app::LengthInfoInterface,
                                                comms::option::app::IdInfoInterface,
                                                comms::option::app::NameInterface>;
export using OutFrame = proto::frame::Frame<OutMessage, std::tuple<>, ServerOptions>;

// Declaration of output messages
export using OutPing = proto::message::Ping<OutMessage>;
export using OutEmergencyStop = proto::message::EmergencyStop<OutMessage>;
export using OutDeviceInfoRequest = proto::message::DeviceInfoRequest<OutMessage>;

export using TransactionId = proto::FrameInterfaceFields::TransactionId;

export template <typename AsyncStream>
class AsyncMachineProtocolServer
{
    log::Logger logger_;
    AsyncStream stream_;
    cobalt::channel<std::vector<uint8_t>> write_queue_;
    using ChannelPtr = std::shared_ptr<cobalt::channel<InFrame::MsgPtr>>;
    std::unordered_map<TransactionId::ValueType, ChannelPtr> dispatch_map_;
    TransactionId::ValueType transaction_id_counter_{0};
    std::unordered_multimap<proto::MsgId, ChannelPtr> event_dispatch_map_;
    bool is_running_ = false;

    struct ShutdownSignal
    {
    };

    boost::cobalt::channel<ShutdownSignal> shutdown_barrier_; // Capacity of 1 (Rendezvous/Buffered)

  public:
    AsyncMachineProtocolServer(AsyncMachineProtocolServer&&) noexcept = delete;
    AsyncMachineProtocolServer& operator=(AsyncMachineProtocolServer&&) noexcept = delete;
    AsyncMachineProtocolServer(const AsyncMachineProtocolServer&) = delete;
    AsyncMachineProtocolServer& operator=(const AsyncMachineProtocolServer&) = delete;

    ~AsyncMachineProtocolServer()
    {
        try {
            if (is_running_) {
                log::critical{logger_,
                              "Object destroyed while asynchronous loops were still running! "
                              "You must co_await client.stop() before releasing the client"};
            }

            // Force close stream and channels synchronously to prevent hanging the io_context
            boost::system::error_code ec;
            stream_.close(ec);
            shutdown_channels();
        }
        catch (const std::exception& ex) {
            log::error{logger_, "Error while descructing the server: {}", ex.what()};
        }
    }

    AsyncMachineProtocolServer(AsyncStream stream)
        : logger_{log::create_or_get("protocol")}
        , stream_{std::move(stream)}
        , write_queue_{10, stream_.get_executor()}
        , shutdown_barrier_{1, stream_.get_executor()}
    {
    }

    cobalt::task<void> run()
    {
        is_running_ = true;
        try {
            co_await boost::cobalt::race(read_loop(), write_loop());
        }
        catch (const boost::system::system_error& e) {
            log::error{logger_, "I/O loops terminated: {}", e.what()};
        }

        // 1. Unwind and close all active dispatch channels
        shutdown_channels();
        is_running_ = false;

        // 2. Write to the barrier channel to signal that we have completely
        // stopped accessing any member variables of this class.
        try {
            co_await shutdown_barrier_.write(ShutdownSignal{});
        }
        catch (...) {
            // Suppress channel closed exceptions if shutdown happened concurrently
        }
    }

    // Explicitly initiates shutdown, aborts I/O, and awaits absolute termination.
    cobalt::promise<void> stop()
    {
        if (!is_running_) {
            co_return;
        }

        // 1. Terminate all outstanding logical dispatch channels
        shutdown_channels();

        // 2. Cancel all pending physical I/O (reads/writes) registered with the OS reactor
        boost::system::error_code ec;
        stream_.close(ec);

        // 3. Await the barrier signal. Once this resumes, we are 100% guaranteed
        // that the background tasks have terminated and will NEVER access 'this' again.
        try {
            co_await shutdown_barrier_.read();
        }
        catch (...) {
            // Ignore channel closure during teardown
        }
    }

    template <typename ExpectedMsg>
    cobalt::promise<std::expected<ExpectedMsg, comms::ErrorStatus>> async_receive(TransactionId::ValueType transaction_id,
                                                                                  std::chrono::milliseconds timeout)
    {
        auto chan = get_or_create_channel(transaction_id);

        // Garbage Collection: Ensures stale/timed-out keys are erased from the map
        // regardless of the exit path (success, timeout, exception, or cancellation)
        CleanupGuard guard{this, transaction_id};

        try {
            InFrame::MsgPtr msg = co_await read_with_timeout(*chan, timeout);
            static_assert(ExpectedMsg::hasStaticMsgId(), "Expected message doesn't have a compile-time message id.");
            if (msg->getId() != ExpectedMsg::staticMsgId()) {
                co_return std::unexpected(comms::ErrorStatus::InvalidMsgId);
            }
            auto* concrete_ptr = dynamic_cast<ExpectedMsg*>(msg.get());
            if (concrete_ptr == nullptr) {
                co_return std::unexpected(comms::ErrorStatus::MsgAllocFailure);
            }
            auto expected_msg = std::move(*concrete_ptr);
            msg.reset();
            co_return std::move(expected_msg);
        }
        catch (const boost::system::system_error& e) {
            if (e.code() == asio::error::operation_aborted) {
                co_return std::unexpected(comms::ErrorStatus::ProtocolError);
            }
            co_return std::unexpected(comms::ErrorStatus::ProtocolError);
        }
    };

    template <typename... ExpectedEvents>
    cobalt::generator<std::variant<std::monostate, ExpectedEvents...>> async_receive_events()
    {
        auto channel = std::make_shared<cobalt::channel<InFrame::MsgPtr>>(10);
        EventSubscriptionGuard guard{this, channel, std::vector{static_cast<proto::MsgId>(ExpectedEvents::staticMsgId())...}};
        co_await cobalt::this_coro::initial;

        while (is_running_) {
            InFrame::MsgPtr msg;
            try {
                msg = co_await channel->read();
            }
            catch (...) {
                break; // Server shut down or channel closed
            }

            std::optional<std::variant<std::monostate, ExpectedEvents...>> matched_event;

            bool _ = ([&]() -> bool {
                if (msg->getId() == ExpectedEvents::staticMsgId()) {
                    if (auto* concrete_ptr = dynamic_cast<ExpectedEvents*>(msg.get())) {
                        matched_event = std::move(*concrete_ptr);
                        return true;
                    }
                }
                return false;
            }() || ...);

            if (matched_event.has_value()) {
                co_yield std::move(*matched_event);
            }
        }
        co_return {};
    }

    cobalt::promise<std::expected<TransactionId::ValueType, comms::ErrorStatus>> async_send(auto msg)
    {
        OutFrame frame;
        std::vector<std::uint8_t> output;

        // Add unique transaction id to frame.
        const auto transaction_id = ++transaction_id_counter_;
        msg.transportField_transactionId().setValue(transaction_id);

        // Use polymorphic serialization length calculation to create
        // buffer of the requires size
        output.resize(frame.length(msg));

        // Serialize message into the buffer (including framing)
        // The serialization uses polymorphic write functionality.
        auto* write_iter = output.data();
        auto es = frame.write(msg, write_iter, output.size());
        if (es != comms::ErrorStatus::Success) {
            co_return std::unexpected{es};
        }

        // write_iter has been advanced, check that it reached end of the allocated buffer.
        assert(output.size() == static_cast<std::size_t>(std::distance(output.data(), write_iter)));

        try {
            co_await write_queue_.write(std::move(output));
            co_return transaction_id;
        }
        catch (...) {
            // Reached only if shutdown_channels() closes the queue
            co_return std::unexpected(comms::ErrorStatus::ProtocolError);
        }
    }

  private:
    cobalt::promise<InFrame::MsgPtr> read_with_timeout(cobalt::channel<InFrame::MsgPtr>& chan, std::chrono::milliseconds timeout)
    {
        boost::asio::steady_timer timer{stream_.get_executor()};
        timer.expires_after(timeout);

        // Race the channel read against the timer
        auto res = co_await cobalt::race(chan.read(), timer.async_wait(cobalt::use_op));

        // If index 1 wins, the timer fired first (Timeout)
        if (res.index() == 1) {
            throw boost::system::system_error(asio::error::operation_aborted);
        }

        co_return std::move(boost::variant2::get<0>(res));
    }

    ChannelPtr get_or_create_channel(proto::FrameInterfaceFields::TransactionId::ValueType id)
    {
        auto it = dispatch_map_.find(id);
        if (it == dispatch_map_.end()) {
            it = dispatch_map_.emplace(id, std::make_shared<cobalt::channel<InFrame::MsgPtr>>(1)).first;
        }
        return it->second;
    }

    void shutdown_channels()
    {
        write_queue_.close();
        for (auto&& [id, chan] : dispatch_map_) {
            chan->close();
        }
        dispatch_map_.clear();

        for (auto&& [msg_id, chan] : event_dispatch_map_) {
            chan->close();
        }
        event_dispatch_map_.clear();
    }

    cobalt::task<void> write_loop()
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
                co_await asio::async_write(stream_,
                                           asio::buffer(data),
                                           asio::cancel_after(std::chrono::milliseconds(500), asio::as_tuple(asio::deferred)));
            }
            catch (const boost::system::system_error& e) {
                log::error{logger_, "Write aborted: {}", e.what()};
                break;
            }
        }
    }

    cobalt::task<void> read_loop()
    {
        static constexpr std::size_t kMaxBufferSize = static_cast<const std::size_t>(64 * 1024);

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
                    log::error{logger_, "Fatal: Buffer limit exceeded. Dropping connection to prevent OOM."};
                    co_return;
                }
                rx_buffer.resize(rx_buffer.size() * 2);
            }

            auto [ec, bytes_read] = co_await stream_.async_read_some(
                asio::buffer(rx_buffer.data() + valid_bytes, rx_buffer.size() - valid_bytes), asio::as_tuple(cobalt::use_op));

            if (ec) {
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

                const auto transaction_id = std::get<proto::FrameInterfaceFields::TransactionId>(msg->transportFields()).value();

                const auto rx_it = dispatch_map_.find(transaction_id);
                if (rx_it != dispatch_map_.end()) {
                    auto chan = rx_it->second;
                    dispatch_map_.erase(rx_it);
                    try {
                        co_await chan->write(std::move(msg));
                    }
                    catch (const boost::system::system_error& write_error) {
                        log::warn{logger_, "Dispatch failed for TX ID {}: {}", transaction_id, write_error.what()};
                    }
                }
                else {
                    const auto msg_id = msg->getId();
                    auto sub_it = event_dispatch_map_.find(msg_id);

                    if (sub_it != event_dispatch_map_.end()) {
                        try {
                            //! TODO: duplicate the message for all interested listeners. Don't know if I need this yet.
                            // Yields to the *first* local channel listening for this specific message.
                            co_await sub_it->second->write(std::move(msg));
                        }
                        catch (...) {
                            log::warn{logger_, "Failed to route event with ID {} to its subscriber.", static_cast<int>(msg_id)};
                        }
                    }
                    else {
                        log::debug{logger_, "Discarding unhandled asynchronous event with ID: {}", static_cast<int>(msg_id)};
                    }
                }

                consume_front(consumed);
            }
        }
    }

    struct CleanupGuard
    {
        AsyncMachineProtocolServer<AsyncStream>* client;
        proto::FrameInterfaceFields::TransactionId::ValueType transaction_id;

        ~CleanupGuard()
        {
            client->dispatch_map_.erase(transaction_id);
        }
    };

    // RAII guard to cleanly manage Subscriptions mapping locally
    struct EventSubscriptionGuard
    {
        AsyncMachineProtocolServer<AsyncStream>* server;
        ChannelPtr channel;
        std::vector<proto::MsgId> ids;

        EventSubscriptionGuard(AsyncMachineProtocolServer<AsyncStream>* s, ChannelPtr c, std::vector<proto::MsgId> i)
            : server(s)
            , channel(std::move(c))
            , ids(std::move(i))
        {
            for (auto id : ids) {
                server->event_dispatch_map_.emplace(id, channel);
            }
        }

        ~EventSubscriptionGuard()
        {
            // Erase specifically this channel's mappings to avoid unregistering parallel listeners
            for (auto id : ids) {
                auto range = server->event_dispatch_map_.equal_range(id);
                for (auto it = range.first; it != range.second;) {
                    if (it->second == channel) {
                        it = server->event_dispatch_map_.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }
        }
    };
};
} // namespace cm
