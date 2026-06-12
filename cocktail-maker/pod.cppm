module;
#include <boost/cobalt.hpp>

export module cm:pod;

import std;
import :pod_types;
import :strong_type;
import :station_state;
import :awaitable_bool;
import :overloaded;
import :async_machine_protocol_server;

namespace cobalt = boost::cobalt;

namespace cm {
template <typename F>
auto retry_on_timeout(std::size_t max_retries, F f) -> decltype(f(std::chrono::milliseconds{}))
{
    size_t retries = 0;
    std::chrono::milliseconds backoff_retry{100};
    for (;;) {
        try {
            co_return co_await f(backoff_retry * (retries + 1));
        }
        catch (const TimeoutError&) {
            // If we hit the max retry limit, rethrow the TimeoutError
            if (retries >= max_retries) {
                throw;
            }
            ++retries;
        }
    }
}

class NoopPodState final : public PodState
{
  public:
    void update_info(PodInfo info) override {};
    void update_state([[maybe_unused]] ConnectionState state) override {};
};

export class IPod
{
  public:
    virtual ~IPod() = default;
    virtual cobalt::task<void> run(std::unique_ptr<PodState> state) = 0;
};

export template <typename AsyncStream>
class Pod : public IPod
{
  public:
    explicit Pod(AsyncStream stream)
        : server_{std::move(stream)}
        , device_ready_{server_.get_executor()}
    {
    }

    cobalt::task<void> run(std::unique_ptr<PodState> state) override
    {
        state_ = std::move(state);

        co_await cobalt::race(monitor_device(), server_.run());
    }

    cobalt::promise<PodInfo> aquire_device_info(std::chrono::milliseconds timeout = std::chrono::milliseconds{100})
    {
        InDeviceInfoResponse msg = co_await send_and_receive<InDeviceInfoResponse>(OutDeviceInfoRequest{}, timeout);
        co_return PodInfo{
            .id = PodId{msg.field_deviceName().getValue()},
            .firmware_version =
                Version{
                    .major = msg.field_firmwareMajor().getValue(),
                    .minor = msg.field_firmwareMinor().getValue(),
                    .patch = msg.field_firmwarePatch().getValue(),
                },
            .num_pumps = msg.field_numPumps().getValue(),
            .num_valves = msg.field_numValves().getValue(),
        };
    }

  private:
    cobalt::task<void> monitor_device()
    {
        struct Cleanup
        {
            Pod& p_;

            ~Cleanup()
            {
                log::debug(p_.logger_, "monitor device loop finished.");
                p_.device_ready_ = false;
                p_.state_->update_state(PodState::ConnectionState::disconnected);
            }
        };

        Cleanup c{*this};
        state_->update_state(PodState::ConnectionState::connecting);
        const auto pod_info = co_await retry_on_timeout(5, [this](auto timeout) { return aquire_device_info(timeout); });
        state_->update_info(pod_info);
        log::info(logger_, "Device '{}' is ready.", pod_info.id);
        state_->update_state(PodState::ConnectionState::connected);
        device_ready_ = true;

        BOOST_COBALT_FOR(auto event, server_.template async_receive_events<InPong>())
        {
            std::visit(detail::Overloaded{
                           [](InPong msg) {},
                           [](std::monostate) {},
                       },
                       std::move(event));
        }

        /*
        Idee:
        Dieser loop verarbeitet events und updated entsprechend die States der hardware.
        ::pump() muss bspw. wissen, wann die pumpe auch wirklich fertig ist.
        Wenn Ausnahmen/Fehler als Event rein kommen, muss dies hier entsprechend an die UI weiter geleitet werden.
        Bspw:
        * Behälter ist leer => UI Dialog auffüllen nach auffüllen Frage bleibt: Wird das Rezept dann abgebrochen oder die Routinen
        dann einfach fortgesetzt?
        * MotorStateUpdate kommt asynchron nach Pump-Ack rein, vllt. wäre statt bool awaitable ein state als awaitable? Quasi,
        dass eine coroutine auf einen bestimmten wert warten kann und in anderen eine excpetion kommt? Bspw. finished wird
        erwartet und MotorFailure kommt, sollte als cancellation genutzt werden.


        */
    }

  private:
    template <typename RxMsg, typename TxMsg>
    auto send_and_receive(TxMsg tx_msg, std::chrono::milliseconds timeout = std::chrono::milliseconds{100})
        -> cobalt::promise<RxMsg>
    {
        const auto transaction_id = server_.generate_new_transaction_id();

        // register queue at first
        auto rx_action = server_.template async_receive<RxMsg>(transaction_id, timeout);

        co_await server_.async_send(std::move(tx_msg), transaction_id);
        co_return co_await rx_action;
    }

  private:
    log::Logger logger_{log::create_or_get("pod")};
    std::unique_ptr<PodState> state_{std::make_unique<NoopPodState>()};
    AsyncMachineProtocolServer<AsyncStream> server_;
    AwaitableBool device_ready_;
};
} // namespace cm
