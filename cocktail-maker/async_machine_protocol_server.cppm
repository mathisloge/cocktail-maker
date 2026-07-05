module;
#include <boost/cobalt/channel.hpp>
#include <boost/cobalt/generator.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/task.hpp>
#include <comms/GenericMessage.h>
#include <comms/dispatch.h>
#include <comms/options.h>
#include <libassert/assert-macros.hpp>
#include <proto/FrameInterface.h>
#include <proto/dispatch/DispatchServerInputMessage.h>
#include <proto/frame/Frame.h>
#include <proto/input/AllMessages.h>
#include <proto/input/ServerInputMessages.h>
#include <proto/options/ServerDefaultOptions.h>

export module cm:async_machine_protocol_server;

import std;
import libassert;
import cm.core;

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
export using InNak = proto::message::Nak<InMessage>;
export using InDeviceInfoResponse = proto::message::DeviceInfoResponse<InMessage>;
export using InPumpFinishedCalibrationResponse = proto::message::PumpFinishedCalibrationResponse<InMessage>;
export using InDispenseFinished = proto::message::DispenseFinished<InMessage>;

export using OutMessage = proto::FrameInterface<comms::option::app::WriteIterator<std::uint8_t*>,
                                                comms::option::app::LengthInfoInterface,
                                                comms::option::app::IdInfoInterface,
                                                comms::option::app::NameInterface>;
export using OutFrame = proto::frame::Frame<OutMessage, std::tuple<>, ServerOptions>;

// Declaration of output messages
export using OutPing = proto::message::Ping<OutMessage>;
export using OutEmergencyStop = proto::message::EmergencyStop<OutMessage>;
export using OutDeviceInfoRequest = proto::message::DeviceInfoRequest<OutMessage>;
export using OutLoadCellCalibrateWithRefWeight = proto::message::LoadCellCalibrateWithRefWeight<OutMessage>;
export using OutLoadCellTare = proto::message::LoadCellTare<OutMessage>;
export using OutPumpStartCalibration = proto::message::PumpStartCalibration<OutMessage>;
export using OutHighlightDispenser = proto::message::HighlightDispenser<OutMessage>;
export using OutDispense = proto::message::Dispense<OutMessage>;

export using TransactionId = proto::FrameInterfaceFields::TransactionId;

export class ProtocolError : public std::runtime_error
{
  public:
    ProtocolError(comms::ErrorStatus error_status, std::string human_readable_what);

    comms::ErrorStatus error_status() const;

  private:
    comms::ErrorStatus error_status_;
};

export class TimeoutError : public std::runtime_error
{
  public:
    using runtime_error::runtime_error;
};

export class AsyncMachineProtocolServer
{
    static constexpr std::size_t kWriteQueueCapacity = 10;
    static constexpr std::size_t kResponseChannelCapacity = 1;
    static constexpr std::size_t kEventChannelCapacity = 10;

    log::Logger logger_;
    std::unique_ptr<AnyIoStream> stream_;
    cobalt::channel<std::vector<uint8_t>> write_queue_;
    using ChannelPtr = std::shared_ptr<cobalt::channel<InFrame::MsgPtr>>;
    std::unordered_map<TransactionId::ValueType, ChannelPtr> dispatch_map_;
    TransactionId::ValueType transaction_id_counter_{0};
    std::unordered_multimap<proto::MsgId, ChannelPtr> event_dispatch_map_;
    bool is_running_ = false;

  public:
    AsyncMachineProtocolServer(AsyncMachineProtocolServer&&) noexcept = delete;
    AsyncMachineProtocolServer& operator=(AsyncMachineProtocolServer&&) noexcept = delete;
    AsyncMachineProtocolServer(const AsyncMachineProtocolServer&) = delete;
    AsyncMachineProtocolServer& operator=(const AsyncMachineProtocolServer&) = delete;

    ~AsyncMachineProtocolServer();

    explicit AsyncMachineProtocolServer(std::unique_ptr<AnyIoStream> stream);

    auto get_executor() -> asio::any_io_executor;

    cobalt::task<void> run();

    TransactionId::ValueType generate_new_transaction_id();

    template <typename ExpectedMsg>
    auto async_receive(TransactionId::ValueType transaction_id, std::chrono::milliseconds timeout)
        -> cobalt::promise<ExpectedMsg>;

    template <typename... ExpectedMsgs>
        requires(sizeof...(ExpectedMsgs) >= 2)
    auto async_receive(TransactionId::ValueType transaction_id, std::chrono::milliseconds timeout)
        -> cobalt::promise<std::variant<ExpectedMsgs...>>;

    template <typename... ExpectedEvents>
    auto async_receive_events() -> cobalt::generator<std::variant<std::monostate, ExpectedEvents...>>;

    template <typename Message>
    auto async_send(Message msg, TransactionId::ValueType transaction_id) -> cobalt::promise<void>;

  private:
    template <typename... ExpectedMsgs>
    auto async_receive_impl(TransactionId::ValueType transaction_id, std::chrono::milliseconds timeout)
        -> cobalt::promise<std::variant<ExpectedMsgs...>>;

    auto read_with_timeout(cobalt::channel<InFrame::MsgPtr>& chan, std::chrono::milliseconds timeout)
        -> cobalt::promise<InFrame::MsgPtr>;

    auto get_or_create_channel(TransactionId::ValueType id) -> ChannelPtr;

    void shutdown_channels();

    cobalt::task<void> write_loop();

    cobalt::task<void> read_loop();

    struct CleanupGuard
    {
        AsyncMachineProtocolServer* client;
        TransactionId::ValueType transaction_id;

        ~CleanupGuard();
    };

    // RAII guard to cleanly manage Subscriptions mapping locally
    struct EventSubscriptionGuard
    {
        AsyncMachineProtocolServer* server;
        ChannelPtr channel;
        std::vector<proto::MsgId> ids;

        EventSubscriptionGuard(AsyncMachineProtocolServer* s, ChannelPtr c, std::vector<proto::MsgId> i);

        ~EventSubscriptionGuard();
    };
};

template <typename ExpectedMsg>
auto AsyncMachineProtocolServer::async_receive(TransactionId::ValueType transaction_id, std::chrono::milliseconds timeout)
    -> cobalt::promise<ExpectedMsg>
{
    co_return std::get<0>(co_await async_receive_impl<ExpectedMsg>(transaction_id, timeout));
}

template <typename... ExpectedMsgs>
    requires(sizeof...(ExpectedMsgs) >= 2)
auto AsyncMachineProtocolServer::async_receive(TransactionId::ValueType transaction_id, std::chrono::milliseconds timeout)
    -> cobalt::promise<std::variant<ExpectedMsgs...>>
{
    co_return co_await async_receive_impl<ExpectedMsgs...>(transaction_id, timeout);
}

template <typename... ExpectedEvents>
auto AsyncMachineProtocolServer::async_receive_events() -> cobalt::generator<std::variant<std::monostate, ExpectedEvents...>>
{
    boost::asio::cancellation_state cs = co_await boost::asio::this_coro::cancellation_state;
    auto channel = std::make_shared<cobalt::channel<InFrame::MsgPtr>>(kEventChannelCapacity);
    EventSubscriptionGuard guard{this, channel, std::vector{static_cast<proto::MsgId>(ExpectedEvents::staticMsgId())...}};

    co_await cobalt::this_coro::initial;
    while (cs.cancelled() == boost::asio::cancellation_type::none) {
        InFrame::MsgPtr msg;
        try {
            msg = co_await channel->read();
        }
        catch (...) {
            break; // Server shut down or channel closed
        }

        std::optional<std::variant<std::monostate, ExpectedEvents...>> matched_event;

        // Fold over the candidate types purely for the short-circuiting side effect;
        // the resulting bool is not otherwise needed.
        (void)([&]() -> bool {
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

template <typename Message>
auto AsyncMachineProtocolServer::async_send(Message msg, TransactionId::ValueType transaction_id) -> cobalt::promise<void>
{
    OutFrame frame;
    std::vector<std::uint8_t> output;

    // Add unique transaction id to frame.
    msg.transportField_transactionId().setValue(transaction_id);

    // Use polymorphic serialization length calculation to create
    // buffer of the requires size
    output.resize(frame.length(msg));

    // Serialize message into the buffer (including framing)
    // The serialization uses polymorphic write functionality.
    auto* write_iter = output.data();
    auto es = frame.write(msg, write_iter, output.size());
    if (es != comms::ErrorStatus::Success) {
        throw ProtocolError{es, "Could not serialize message into buffer."};
    }

    // write_iter has been advanced, check that it reached end of the allocated buffer.
    ASSERT(output.size() == static_cast<std::size_t>(std::distance(output.data(), write_iter)));

    log::trace(logger_, "Schedule message {} with transaction id '{}'", msg.name(), transaction_id);
    co_await write_queue_.write(std::move(output));
    co_return;
}

template <typename... ExpectedMsgs>
auto AsyncMachineProtocolServer::async_receive_impl(TransactionId::ValueType transaction_id, std::chrono::milliseconds timeout)
    -> cobalt::promise<std::variant<ExpectedMsgs...>>
{
    static_assert((ExpectedMsgs::hasStaticMsgId() && ...), "All expected messages must have a compile-time message id.");

    auto chan = get_or_create_channel(transaction_id);
    CleanupGuard guard{this, transaction_id};

    InFrame::MsgPtr msg = co_await read_with_timeout(*chan, timeout);

    std::optional<std::variant<ExpectedMsgs...>> result;

    auto try_match = [&]<typename Msg>() {
        if (result.has_value() || msg->getId() != Msg::staticMsgId()) {
            return;
        }
        auto* concrete_ptr = dynamic_cast<Msg*>(msg.get());
        if (concrete_ptr == nullptr) {
            throw ProtocolError{comms::ErrorStatus::MsgAllocFailure, "Could not cast message to expected message."};
        }
        result = std::variant<ExpectedMsgs...>{std::in_place_type<Msg>, std::move(*concrete_ptr)};
    };

    (try_match.template operator()<ExpectedMsgs>(), ...);

    if (!result.has_value()) {
        throw ProtocolError{comms::ErrorStatus::InvalidMsgId,
                            std::format("Expected message doesn't match with received message id({}).", msg->getId())};
    }

    msg.reset();
    co_return std::move(*result);
}

} // namespace cm
