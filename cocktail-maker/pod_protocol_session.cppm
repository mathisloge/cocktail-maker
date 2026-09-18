module;
#include <boost/asio/any_io_executor.hpp>
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
#include <spdlog/spdlog.h>

export module cm:pod_protocol_session;

import std;
import libassert;
import cm.core;

namespace cm {

namespace asio = boost::asio;

using ServerOptions = proto::options::ServerDefaultOptions;
using InMessage = proto::FrameInterface<comms::option::app::ReadIterator<const std::uint8_t*>,
                                        comms::option::app::LengthInfoInterface,
                                        comms::option::app::IdInfoInterface,
                                        comms::option::app::NameInterface>;

using InFrame = proto::frame::Frame<InMessage, proto::input::ServerInputMessages<InMessage>, ServerOptions>;

// All messages received from a Pod
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

export class PodProtocolSession
{
    static constexpr std::size_t kWriteQueueCapacity = 10;
    static constexpr std::size_t kResponseChannelCapacity = 1;

    using ResponseChannel = Channel<InFrame::MsgPtr>;
    using ChannelPtr = std::shared_ptr<ResponseChannel>;

    log::Logger logger_;
    std::unique_ptr<AnyIoStream> stream_;
    Channel<std::vector<uint8_t>> write_queue_;
    std::unordered_map<TransactionId::ValueType, ChannelPtr> dispatch_map_;
    TransactionId::ValueType transaction_id_counter_{0};
    bool is_running_ = false;

  public:
    PodProtocolSession(PodProtocolSession&&) noexcept = delete;
    PodProtocolSession& operator=(PodProtocolSession&&) noexcept = delete;
    PodProtocolSession(const PodProtocolSession&) = delete;
    PodProtocolSession& operator=(const PodProtocolSession&) = delete;

    ~PodProtocolSession();

    explicit PodProtocolSession(std::unique_ptr<AnyIoStream> stream);

    auto get_executor() -> asio::any_io_executor;

    Task<void> run();

    TransactionId::ValueType generate_new_transaction_id();

    /**
     * Waits for the response with `transaction_id`.
     *
     * The response is registered right away when this is called, not when the returned task is started, so the request can
     * be sent in between without its response getting lost. The timeout starts once the task is awaited.
     */
    template <typename ExpectedMsg>
    auto async_receive(TransactionId::ValueType transaction_id, std::chrono::milliseconds timeout) -> Task<ExpectedMsg>;

    template <typename... ExpectedMsgs>
        requires(sizeof...(ExpectedMsgs) >= 2)
    auto async_receive(TransactionId::ValueType transaction_id, std::chrono::milliseconds timeout)
        -> Task<std::variant<ExpectedMsgs...>>;

    template <typename Message>
    auto async_send(Message msg, TransactionId::ValueType transaction_id) -> Task<void>;

  private:
    /// Keeps the response channel of one transaction registered in the dispatch map for as long as it lives.
    class ResponseRegistration
    {
      public:
        ResponseRegistration(PodProtocolSession& session, TransactionId::ValueType transaction_id, ChannelPtr channel);
        ResponseRegistration(ResponseRegistration&& other) noexcept;
        ResponseRegistration& operator=(ResponseRegistration&&) = delete;
        ResponseRegistration(const ResponseRegistration&) = delete;
        ResponseRegistration& operator=(const ResponseRegistration&) = delete;
        ~ResponseRegistration();

        [[nodiscard]] ResponseChannel& channel() const;

      private:
        PodProtocolSession* session_;
        TransactionId::ValueType transaction_id_;
        ChannelPtr channel_;
    };

    template <typename ExpectedMsg>
    auto receive_single(ResponseRegistration registration, std::chrono::milliseconds timeout) -> Task<ExpectedMsg>;

    template <typename... ExpectedMsgs>
    auto receive_matching(ResponseRegistration registration, std::chrono::milliseconds timeout)
        -> Task<std::variant<ExpectedMsgs...>>;

    auto read_with_timeout(ResponseChannel& chan, std::chrono::milliseconds timeout) -> Task<InFrame::MsgPtr>;

    auto register_response(TransactionId::ValueType id) -> ResponseRegistration;

    void shutdown_channels();

    Task<void> write_loop();

    Task<void> read_loop();
};

template <typename ExpectedMsg>
auto PodProtocolSession::async_receive(TransactionId::ValueType transaction_id, std::chrono::milliseconds timeout)
    -> Task<ExpectedMsg>
{
    return receive_single<ExpectedMsg>(register_response(transaction_id), timeout);
}

template <typename... ExpectedMsgs>
    requires(sizeof...(ExpectedMsgs) >= 2)
auto PodProtocolSession::async_receive(TransactionId::ValueType transaction_id, std::chrono::milliseconds timeout)
    -> Task<std::variant<ExpectedMsgs...>>
{
    return receive_matching<ExpectedMsgs...>(register_response(transaction_id), timeout);
}

template <typename Message>
auto PodProtocolSession::async_send(Message msg, TransactionId::ValueType transaction_id) -> Task<void>
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

    SPDLOG_LOGGER_TRACE(logger_, "Schedule message {} with transaction id '{}'", msg.name(), transaction_id);
    co_await cm::async_send(write_queue_, std::move(output));
}

template <typename ExpectedMsg>
auto PodProtocolSession::receive_single(ResponseRegistration registration, std::chrono::milliseconds timeout) -> Task<ExpectedMsg>
{
    auto result = co_await receive_matching<ExpectedMsg>(std::move(registration), timeout);
    co_return std::get<0>(std::move(result));
}

template <typename... ExpectedMsgs>
auto PodProtocolSession::receive_matching(ResponseRegistration registration, std::chrono::milliseconds timeout)
    -> Task<std::variant<ExpectedMsgs...>>
{
    static_assert((ExpectedMsgs::hasStaticMsgId() && ...), "All expected messages must have a compile-time message id.");

    InFrame::MsgPtr msg = co_await read_with_timeout(registration.channel(), timeout);

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
