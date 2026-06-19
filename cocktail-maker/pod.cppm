module;
#include <boost/cobalt.hpp>

export module cm:pod;

import std;
import mp_units;
import cm.core;
import :pod_types;
import :station_state;
import :async_machine_protocol_server;
import :dispenser;

namespace cobalt = boost::cobalt;

using namespace std::chrono_literals;

namespace cm {
export class IPod
{
  public:
    IPod() = default;
    IPod(const IPod&) = delete ("Unique instances with non-copyable/movable state expected.");
    IPod(IPod&&) noexcept = delete;
    IPod& operator=(const IPod&) = delete;
    IPod& operator=(IPod&&) noexcept = delete;
    virtual ~IPod() = default;

    virtual PodId pod_id() const = 0;
    virtual cobalt::task<void> run(std::unique_ptr<PodState> state) = 0;
    virtual std::expected<std::unique_ptr<Dispenser>, DispenserNotFoundError> create_dispenser(DispenserId dispenser_id) = 0;

    virtual cobalt::promise<void> load_cell_reset_offset(DispenserId dispenser_id) = 0;
    virtual cobalt::promise<void> load_cell_set_ref_weight(DispenserId dispenser_id, units::Grams grams) = 0;
};

class DispenserPodImpl : public Dispenser
{
  public:
    explicit DispenserPodImpl(std::weak_ptr<IPod> pod, DispenserId dispenser_id)
        : dispenser_id_{dispenser_id}
        , pod_{std::move(pod)}
    {
    }

    cobalt::promise<void> load_cell_reset_offset() override
    {
        co_await pod()->load_cell_reset_offset(dispenser_id_);
    }

    cobalt::promise<void> load_cell_set_ref_weight(units::Grams grams) override
    {
        co_await pod()->load_cell_set_ref_weight(dispenser_id_, grams);
    }

  protected:
    std::shared_ptr<IPod> pod()
    {
        auto p = pod_.lock();
        if (p == nullptr) {
            throw std::runtime_error("Pod lifetime exceeded");
        }
        return p;
    }

    DispenserId id() const
    {
        return dispenser_id_;
    }

  private:
    const DispenserId dispenser_id_;
    std::weak_ptr<IPod> pod_;
};

export class Pump final : public DispenserPodImpl
{
    log::Logger logger_;

  public:
    Pump(std::weak_ptr<IPod> pod, DispenserId dispenser_id)
        : DispenserPodImpl{std::move(pod), dispenser_id}
        , logger_{log::create_or_get(std::format("pump_{}", dispenser_id))}
    {
    }

    cobalt::promise<void> dispense(units::Litre volume) override
    {
        log::debug(logger_, "Start dispense of {}.", volume);
        co_return;
    }
};

export class Valve final : public DispenserPodImpl
{
    DispenserId id_;
    log::Logger logger_;

  public:
    Valve(std::weak_ptr<IPod> pod, DispenserId dispenser_id)
        : DispenserPodImpl{std::move(pod), dispenser_id}
        , logger_{log::create_or_get(std::format("valve_{}", dispenser_id))}
    {
    }

    cobalt::promise<void> dispense(units::Litre volume) override
    {
        log::debug(logger_, "Start dispense of {}.", volume);
        co_return;
    }
};

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

export template <typename AsyncStream>
class Pod : public IPod, public std::enable_shared_from_this<Pod<AsyncStream>>
{
  public:
    explicit Pod(AsyncStream stream)
        : server_{std::move(stream)}
        , device_ready_{server_.get_executor()}
    {
    }

    PodId pod_id() const override
    {
        return state_->info().id;
    }

    cobalt::task<void> run(std::unique_ptr<PodState> state) override
    {
        state_ = std::move(state);
        co_await cobalt::race(server_.run(), monitor_device(), keep_alive());
    }

    std::expected<std::unique_ptr<Dispenser>, DispenserNotFoundError> create_dispenser(DispenserId dispenser_id) override
    {
        const auto info = state_->info();
        if (dispenser_id.raw() >= (info.num_pumps + info.num_valves)) {
            return std::unexpected{DispenserNotFoundError{"Searched dispenser id exceeded pumps and valves"}};
        }
        if (dispenser_id.raw() >= info.num_pumps) {
            return std::make_unique<Valve>(this->shared_from_this(), dispenser_id);
        }
        return std::make_unique<Pump>(this->shared_from_this(), dispenser_id);
    }

    cobalt::promise<PodInfo> aquire_device_info(std::chrono::milliseconds timeout = 100ms)
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

    cobalt::promise<void> load_cell_reset_offset(DispenserId dispenser_id) override
    {
        auto rx = OutLoadCellResetOffset{};
        rx.field_dispenserId().setValue(dispenser_id.raw());
        [[maybe_unused]] auto ack = co_await send_and_receive<InAck>(std::move(rx), 500ms);
    }

    cobalt::promise<void> load_cell_set_ref_weight(DispenserId dispenser_id, units::Grams grams) override
    {
        auto rx = OutLoadCellSetRefWeight{};
        rx.field_dispenserId().setValue(dispenser_id.raw());
        rx.field_gram().setValue(grams.numerical_value_in(units::si::gram));
        [[maybe_unused]] auto ack = co_await send_and_receive<InAck>(std::move(rx), 500ms);
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

    cobalt::task<void> keep_alive()
    {
        auto cs = co_await boost::asio::this_coro::cancellation_state;
        co_await device_ready_; // wait until the device info was send.
        while (cs.cancelled() == boost::asio::cancellation_type::none) {
            // will be cancelled if the pong wasn't received in x ms.
            co_await send_and_receive<InPong>(OutPing{}, std::chrono::milliseconds{2000});
        }
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
