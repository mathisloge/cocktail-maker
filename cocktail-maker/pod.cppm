module;
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt.hpp>
#include <proto/field/ErrorCodeCommon.h>
#include <proto/field/LedEffectCommon.h>

export module cm:pod;

import std;
import mp_units;
import cm.core;
import :pod_types;
import :station_state;
import :async_machine_protocol_server;
import :dispenser;

namespace cobalt = boost::cobalt;
namespace asio = boost::asio;

using namespace std::chrono_literals;

namespace cm {
export class PodReceiveError : public std::runtime_error
{
  public:
    explicit PodReceiveError(proto::field::ErrorCodeVal error)
        : runtime_error{std::format("NAK with error reason: {}", proto::field::ErrorCodeCommon::valueName(error))}
    {
    }
};

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

    virtual cobalt::promise<void> highlight_dispenser(DispenserId dispenser_id, std::chrono::milliseconds duration) = 0;

    virtual cobalt::promise<void> load_cell_calibrate_with_ref_weight(DispenserId dispenser_id, units::Grams grams) = 0;
    virtual cobalt::promise<void> load_cell_tare(DispenserId dispenser_id) = 0;

    virtual cobalt::promise<units::Litre> dispense(DispenserId dispenser_id, units::Litre volume) = 0;

    virtual cobalt::promise<units::Litre> pump_calibrate(DispenserId dispenser_id, units::Steps steps) = 0;
    virtual cobalt::task<void> force_safe_state() = 0;
};

class DispenserPodImpl : public Dispenser
{
  public:
    explicit DispenserPodImpl(std::weak_ptr<IPod> pod, DispenserId dispenser_id, std::string logger_name)
        : dispenser_id_{dispenser_id}
        , pod_{std::move(pod)}
        , logger_{log::create_or_get(std::move(logger_name))}
    {
    }

    cobalt::promise<units::Litre> dispense(units::Litre volume) override
    {
        log::debug(logger_, "Start dispense of {}.", volume);
        const auto measured_volume = co_await pod()->dispense(dispenser_id_, volume);
        log::debug(logger_, "Finished dispensing of {}. Dispensed: {}.", volume, measured_volume);
        co_return measured_volume;
    }

    cobalt::promise<void> load_cell_calibrate_with_ref_weight(units::Grams grams) override
    {
        log::debug(logger_, "Set load cell ref weight to {}.", grams);
        co_await pod()->load_cell_calibrate_with_ref_weight(dispenser_id_, grams);
    }

    cobalt::promise<void> load_cell_tare() override
    {
        log::debug(logger_, "load cell TARE.");
        co_await pod()->load_cell_tare(dispenser_id_);
    }

    cobalt::promise<void> highlight(std::chrono::milliseconds duration) override
    {
        log::debug(logger_, "Highlight for {}.", duration);
        co_await pod()->highlight_dispenser(id(), duration);
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

  protected:
    log::Logger logger_;

  private:
    const DispenserId dispenser_id_;
    std::weak_ptr<IPod> pod_;
};

export class Pump final : public DispenserPodImpl
{

  public:
    Pump(std::weak_ptr<IPod> pod, DispenserId dispenser_id)
        : DispenserPodImpl{std::move(pod), dispenser_id, std::format("pump_{}", dispenser_id)}
    {
    }

    cobalt::promise<units::Litre> calibrate(units::Steps steps)
    {
        log::debug(logger_, "Start calibration with {}.", steps);
        co_return co_await pod()->pump_calibrate(id(), steps);
    }
};

export class Valve final : public DispenserPodImpl
{
  public:
    Valve(std::weak_ptr<IPod> pod, DispenserId dispenser_id)
        : DispenserPodImpl{std::move(pod), dispenser_id, std::format("valve_{}", dispenser_id)}
    {
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

export class Pod : public IPod, public std::enable_shared_from_this<Pod>
{
  public:
    explicit Pod(std::unique_ptr<AnyIoStream> stream)
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

    cobalt::promise<void> load_cell_calibrate_with_ref_weight(const DispenserId dispenser_id, const units::Grams grams) override
    {
        auto tx = OutLoadCellCalibrateWithRefWeight{};
        tx.field_dispenserId().setValue(dispenser_id.raw());
        tx.field_gram().setValue(grams.numerical_value_in(units::si::gram));
        co_await send_with_ack(std::move(tx), 500ms);
    }

    cobalt::promise<void> load_cell_tare(DispenserId dispenser_id) override
    {
        auto tx = OutLoadCellTare{};
        tx.field_dispenserId().setValue(dispenser_id.raw());
        co_await send_with_ack(std::move(tx), 500ms);
    }

    cobalt::promise<units::Litre> dispense(DispenserId dispenser_id, units::Litre volume) override
    {
        auto tx = OutDispense{};
        tx.field_dispenserId().setValue(dispenser_id.raw());
        tx.field_millilitre().setValue(volume.numerical_value_in(units::milli_litre));

        const InDispenseFinished finish_result = co_await send_action_with_response<InDispenseFinished>(std::move(tx), 30s);
        co_return (finish_result.field_millilitre().value() * units::milli_litre);
    }

    cobalt::promise<units::Litre> pump_calibrate(const DispenserId dispenser_id, const units::Steps steps) override
    {
        auto tx = OutPumpStartCalibration{};
        tx.field_dispenserId().setValue(dispenser_id.raw());
        tx.field_pumpStep().setValue(steps);

        const InPumpFinishedCalibrationResponse finish_result =
            co_await send_action_with_response<InPumpFinishedCalibrationResponse>(std::move(tx), (steps * 2ms) + 100ms);
        co_return finish_result.field_millilitre().value() * units::milli_litre;
    }

    cobalt::promise<void> highlight_dispenser(DispenserId dispenser_id, std::chrono::milliseconds duration) override
    {
        auto tx = OutHighlightDispenser{};
        tx.field_dispenserId().setValue(dispenser_id.raw());
        tx.field_ledEffect().setValue(proto::field::LedEffectVal::Highlight);
        tx.field_milliseconds().setValue(duration.count());
        co_await send_with_ack(std::move(tx), 100ms);
    }

    cobalt::task<void> force_safe_state() override
    {
        co_await send_with_ack(OutEmergencyStop{}, 100ms);
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

        BOOST_COBALT_FOR(auto event, server_.async_receive_events<InPong>())
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
        auto cs = co_await asio::this_coro::cancellation_state;
        co_await device_ready_;
        asio::steady_timer timer{server_.get_executor()};
        while (cs.cancelled() == asio::cancellation_type::none) {
            co_await send_and_receive<InPong>(OutPing{}, 1s);
            timer.expires_after(2s);
            co_await timer.async_wait(cobalt::use_op);
        }
    }

  private:
    template <typename TxMsg>
    auto send_with_ack(TxMsg tx_msg, std::chrono::milliseconds timeout = 100ms) -> cobalt::promise<TransactionId::ValueType>
    {
        const auto transaction_id = server_.generate_new_transaction_id();

        // register queue at first
        auto rx_action = server_.async_receive<InAck, InNak>(transaction_id, timeout);

        co_await server_.async_send(std::move(tx_msg), transaction_id);
        const auto nak_or_ack = co_await rx_action;
        if (std::holds_alternative<InNak>(nak_or_ack)) {
            process_nak(std::get<InNak>(nak_or_ack));
        }
        co_return transaction_id;
    }

    template <typename RxMsg, typename TxMsg>
    auto send_and_receive(TxMsg tx_msg, std::chrono::milliseconds timeout = 100ms) -> cobalt::promise<RxMsg>
    {
        const auto transaction_id = server_.generate_new_transaction_id();

        // register queue at first
        auto rx_action = server_.async_receive<RxMsg>(transaction_id, timeout);

        co_await server_.async_send(std::move(tx_msg), transaction_id);
        co_return co_await rx_action;
    }

    template <typename RxMsg, typename TxMsg>
    auto send_action_with_response(TxMsg tx_msg, std::chrono::milliseconds action_timeout) -> cobalt::promise<RxMsg>
    {
        const auto transaction_id = co_await send_with_ack(std::move(tx_msg));
        const auto nak_or_action = co_await server_.async_receive<RxMsg, InNak>(transaction_id, action_timeout);
        if (std::holds_alternative<InNak>(nak_or_action)) {
            process_nak(std::get<InNak>(nak_or_action));
        }
        co_return std::get<RxMsg>(nak_or_action);
    }

    void process_nak(const InNak& nak)
    {
        switch (nak.field_errorCode().value()) {
        case proto::field::ErrorCodeCommon::ValueType::DispenserEmpty:
            throw DispenserEmptyError{"Dispenser empty."}; // TODO: maybe this should contain the pod and dispenser id?
        case proto::field::ErrorCodeCommon::ValueType::InvalidParameter:
        case proto::field::ErrorCodeCommon::ValueType::DispenserNotFound:
        case proto::field::ErrorCodeCommon::ValueType::UnsupportedInCurrentState:
        case proto::field::ErrorCodeCommon::ValueType::HardwareFault:
        case proto::field::ErrorCodeCommon::ValueType::Busy:
        case proto::field::ErrorCodeCommon::ValueType::NotCalibrated:
        case proto::field::ErrorCodeCommon::ValueType::InternalError:
        case proto::field::ErrorCodeCommon::ValueType::ValuesLimit:
            throw PodReceiveError{nak.field_errorCode().value()};
        }
        std::unreachable();
    }

  private:
    log::Logger logger_{log::create_or_get("pod")};
    std::unique_ptr<PodState> state_{std::make_unique<NoopPodState>()};
    AsyncMachineProtocolServer server_;
    AwaitableBool device_ready_;
};
} // namespace cm
