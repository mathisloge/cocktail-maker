module;
#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <comms/process.h>
#include <proto/FrameInterface.h>
#include <proto/dispatch/DispatchClientInputMessage.h>
#include <proto/frame/Frame.h>
#include <proto/input/AllMessages.h>
#include <proto/input/ClientInputMessages.h>
#include <proto/options/ClientDefaultOptions.h>

export module cm.sim:client;
import std;
import cm.core;
import cm;

using namespace std::chrono_literals;
namespace cobalt = boost::cobalt;
namespace asio = boost::asio;

cobalt::promise<void> delay(std::chrono::milliseconds duration = std::chrono::milliseconds{100})
{
    asio::steady_timer tim{co_await asio::this_coro::executor, duration};
    co_await tim.async_wait(cobalt::use_op);
}

namespace cm::sim {
export using Socket = asio::local::stream_protocol::socket;

export template <typename TSocket>
class Client
{
  private:
    using Options = proto::options::ClientDefaultOptions;
    using Message = proto::FrameInterface<comms::option::app::ReadIterator<const std::uint8_t*>,
                                          comms::option::app::WriteIterator<std::uint8_t*>,
                                          comms::option::app::LengthInfoInterface,
                                          comms::option::app::IdInfoInterface,
                                          comms::option::app::NameInterface,
                                          comms::option::app::Handler<Client>>;

    PROTO_ALIASES_FOR_CLIENT_INPUT_MESSAGES_DEFAULT_OPTIONS(InClient, , Message);
    using ClientFrame = proto::frame::Frame<Message, proto::input::ClientInputMessages<Message, Options>, Options>;

    using DeviceInfoResponse = proto::message::DeviceInfoResponse<Message, Options>;
    using Pong = proto::message::Pong<Message, Options>;
    using Ack = proto::message::Ack<Message, Options>;
    using Nak = proto::message::Nak<Message, Options>;
    using PumpCalibrationFinished = proto::message::PumpFinishedCalibrationResponse<Message, Options>;

    log::Logger logger_;
    TSocket stream_;
    DeviceInfoResponse device_info_response_msg_{};
    cobalt::channel<std::vector<uint8_t>> write_queue_;
    ClientFrame frame_;

  public:
    Client(TSocket socket, std::string name, Version firmware_version)
        : logger_{log::create_or_get(std::format("SimPod_{}", name))}
        , stream_{std::move(socket)}
    {
        device_info_response_msg_.field_deviceName().setValue(std::move(name));
        device_info_response_msg_.field_firmwareMajor().setValue(firmware_version.major);
        device_info_response_msg_.field_firmwareMinor().setValue(firmware_version.minor);
        device_info_response_msg_.field_firmwarePatch().setValue(firmware_version.patch);
        device_info_response_msg_.field_numPumps().setValue(5);
        device_info_response_msg_.field_numValves().setValue(3);
    }

    TSocket& socket()
    {
        return stream_;
    }

    cobalt::detached run()
    {
        try {
            co_await boost::cobalt::race(read_loop(), write_loop());
        }
        catch (const boost::system::system_error& e) {
            log::error{logger_, "I/O loops terminated: {}", e.what()};
        }
    }

    void handle(InClientDeviceInfoRequest& msg)
    {
        async_handle(msg);
    }

    void handle(InClientPing& msg)
    {
        async_handle(msg);
    }

    void handle(InClientLoadCellResetOffset& msg)
    {
        async_handle(msg);
    }

    void handle(InClientLoadCellSetRefWeight& msg)
    {
        async_handle(msg);
    }

    void handle(InClientPumpStartCalibration& msg)
    {
        async_handle(msg);
    }

    void handle(Message& msg)
    {
        log::warn(logger_, "Got unexpected message '{}'", msg.name());
    }

  private:
    cobalt::detached async_handle(InClientDeviceInfoRequest msg)
    {
        co_await delay(400ms);
        co_await async_send(device_info_response_msg_, msg.transportField_transactionId().getValue());
    }

    cobalt::detached async_handle(InClientEmergencyStop msg)
    {
        co_await delay();
    }

    cobalt::detached async_handle(InClientPing msg)
    {
        co_await delay(500ms);
        co_await async_send(Pong{}, msg.transportField_transactionId().getValue());
    }

    cobalt::detached async_handle(InClientLoadCellResetOffset msg)
    {
        co_await delay(150ms);
        co_await async_send(Ack{}, msg.transportField_transactionId().getValue());
    }

    cobalt::detached async_handle(InClientLoadCellSetRefWeight msg)
    {
        co_await delay(150ms);
        co_await async_send(Ack{}, msg.transportField_transactionId().getValue());
    }

    cobalt::detached async_handle(InClientPumpStartCalibration msg)
    {
        const auto trid = msg.transportField_transactionId().getValue();
        co_await delay(50ms);
        co_await async_send(Ack{}, trid);
        co_await delay((msg.field_pumpStep().value() * 1ms) + 50ms);

        PumpCalibrationFinished resp{};
        resp.field_millilitre().setValue(230);
        resp.field_pumpStep().setValue(430);
        co_await async_send(std::move(resp), trid);
    }

  private:
    auto async_send(auto msg, const TransactionId::ValueType transaction_id) -> cobalt::promise<void>
    {
        log::trace(logger_, "Schedule send {} with transaction id '{}'", msg.name(), transaction_id);
        ClientFrame frame;
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
        assert(output.size() == static_cast<std::size_t>(std::distance(output.data(), write_iter)));
        co_await write_queue_.write(std::move(output));
        co_return;
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
                log::debug(logger_, "Could not read from stream. Returning from read-loop. Reason: {}", ec.message());
                co_return;
            }

            valid_bytes += bytes_read;
            const std::uint8_t* iter = rx_buffer.data();
            const auto consumed = comms::processAllWithDispatchViaDispatcher<proto::dispatch::ClientInputMsgDispatcher<>>(
                iter, valid_bytes, frame_, *this);
            consume_front(consumed);
        }
        co_return;
    }
};

} // namespace cm::sim
