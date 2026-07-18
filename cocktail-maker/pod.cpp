module;
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt/async_for.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/race.hpp>
#include <boost/cobalt/task.hpp>
#include <proto/field/ErrorCodeCommon.h>
#include <proto/field/LedEffectCommon.h>
#include <spdlog/spdlog.h>

module cm:pod_impl;
import std;
import cm.core;
import :pod;

namespace cobalt = boost::cobalt;
namespace asio = boost::asio;

namespace cm {

DispenserPodImpl::DispenserPodImpl(std::weak_ptr<IPod> pod, DispenserId dispenser_id, std::string logger_name)
    : dispenser_id_{dispenser_id}
    , pod_{std::move(pod)}
    , logger_{log::create_or_get(std::move(logger_name))}
{
}

cobalt::promise<units::Litre> DispenserPodImpl::dispense(units::Litre volume)
{
    SPDLOG_LOGGER_DEBUG(logger_, "Start dispense of {}.", volume);
    const auto measured_volume = co_await pod()->dispense(dispenser_id_, volume);
    SPDLOG_LOGGER_DEBUG(logger_, "Finished dispensing of {}. Dispensed: {}.", volume, measured_volume);
    co_return measured_volume;
}

cobalt::promise<void> DispenserPodImpl::load_cell_calibrate_with_ref_weight(units::Grams grams)
{
    SPDLOG_LOGGER_DEBUG(logger_, "Set load cell ref weight to {}.", grams);
    co_await pod()->load_cell_calibrate_with_ref_weight(dispenser_id_, grams);
}

cobalt::promise<void> DispenserPodImpl::load_cell_tare()
{
    SPDLOG_LOGGER_DEBUG(logger_, "load cell TARE.");
    co_await pod()->load_cell_tare(dispenser_id_);
}

cobalt::promise<void> DispenserPodImpl::highlight(std::chrono::milliseconds duration)
{
    SPDLOG_LOGGER_DEBUG(logger_, "Highlight for {}.", duration);
    co_await pod()->highlight_dispenser(id(), duration);
}

std::shared_ptr<IPod> DispenserPodImpl::pod()
{
    auto p = pod_.lock();
    if (p == nullptr) {
        throw std::runtime_error("Pod lifetime exceeded");
    }
    return p;
}

DispenserId DispenserPodImpl::id() const
{
    return dispenser_id_;
}

Pump::Pump(std::weak_ptr<IPod> pod, DispenserId dispenser_id)
    : DispenserPodImpl{std::move(pod), dispenser_id, std::format("pump_{}", dispenser_id)}
{
}

cobalt::promise<units::Litre> Pump::calibrate(units::Steps steps)
{
    SPDLOG_LOGGER_DEBUG(logger_, "Start calibration with {}.", steps);
    co_return co_await pod()->pump_calibrate(id(), steps);
}

Valve::Valve(std::weak_ptr<IPod> pod, DispenserId dispenser_id)
    : DispenserPodImpl{std::move(pod), dispenser_id, std::format("valve_{}", dispenser_id)}
{
}

struct MessageEnvironment
{
    PodId pod_id = PodId{};
    DispenserId dispenser_id = DispenserId{};
    std::chrono::milliseconds timeout = 100ms;
};

MessageEnvironment dispenser_env(Pod* pod, DispenserId dispenser_id)
{
    return MessageEnvironment{.pod_id = pod->pod_id(), .dispenser_id = dispenser_id};
}

MessageEnvironment default_env(Pod* pod, std::chrono::milliseconds timeout = 100ms)
{
    return MessageEnvironment{.pod_id = pod->pod_id(), .timeout = timeout};
}

void process_nak(const InNak& nak, const MessageEnvironment& env)
{
    switch (nak.field_errorCode().value()) {
    case proto::field::ErrorCodeCommon::ValueType::DispenserEmpty:
        throw DispenserEmptyError{env.pod_id, env.dispenser_id};
    case proto::field::ErrorCodeCommon::ValueType::DispenserNotFound:
        // throw DispenserEmptyError{pod_id, dispenser_id};
    case proto::field::ErrorCodeCommon::ValueType::InvalidParameter:
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

template <typename TxMsg>
auto send_with_ack(AsyncMachineProtocolServer& server, TxMsg tx_msg, const MessageEnvironment env)
    -> cobalt::promise<TransactionId::ValueType>
{
    const auto transaction_id = server.generate_new_transaction_id();

    // register queue at first
    auto rx_action = server.async_receive<InAck, InNak>(transaction_id, env.timeout);

    co_await server.async_send(std::move(tx_msg), transaction_id);
    const auto nak_or_ack = co_await rx_action;
    if (std::holds_alternative<InNak>(nak_or_ack)) {
        process_nak(std::get<InNak>(nak_or_ack), env);
    }
    co_return transaction_id;
}

template <typename RxMsg, typename TxMsg>
auto send_and_receive(AsyncMachineProtocolServer& server, TxMsg tx_msg, const MessageEnvironment env) -> cobalt::promise<RxMsg>
{
    const auto transaction_id = server.generate_new_transaction_id();

    // register queue at first
    auto rx_action = server.async_receive<RxMsg>(transaction_id, env.timeout);

    co_await server.async_send(std::move(tx_msg), transaction_id);
    co_return co_await rx_action;
}

template <typename RxMsg, typename TxMsg>
auto send_action_with_response(AsyncMachineProtocolServer& server,
                               TxMsg tx_msg,
                               std::chrono::milliseconds action_timeout,
                               const MessageEnvironment env) -> cobalt::promise<RxMsg>
{
    const auto transaction_id = co_await send_with_ack(server, std::move(tx_msg), env);
    const auto nak_or_action = co_await server.async_receive<RxMsg, InNak>(transaction_id, action_timeout);
    if (std::holds_alternative<InNak>(nak_or_action)) {
        process_nak(std::get<InNak>(nak_or_action), env);
    }
    co_return std::get<RxMsg>(nak_or_action);
}

Pod::Pod(std::unique_ptr<AnyIoStream> stream)
    : server_{std::move(stream)}
    , device_ready_{server_.get_executor()}
{
}

PodId Pod::pod_id() const
{
    return state_->info().id;
}

cobalt::task<void> Pod::run(std::unique_ptr<PodState> state)
{
    state_ = std::move(state);
    co_await cobalt::race(server_.run(), monitor_device());
}

std::expected<std::unique_ptr<Dispenser>, DispenserNotFoundError> Pod::create_dispenser(DispenserId dispenser_id)
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

cobalt::promise<PodInfo> Pod::aquire_device_info(std::chrono::milliseconds timeout)
{
    InDeviceInfoResponse msg =
        co_await send_and_receive<InDeviceInfoResponse>(server_, OutDeviceInfoRequest{}, default_env(this, timeout));
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

cobalt::promise<void> Pod::load_cell_calibrate_with_ref_weight(const DispenserId dispenser_id, const units::Grams grams)
{
    auto tx = OutLoadCellCalibrateWithRefWeight{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_gram().setValue(grams.numerical_value_in(units::si::gram));
    co_await send_with_ack(server_, std::move(tx), dispenser_env(this, dispenser_id));
}

cobalt::promise<void> Pod::load_cell_tare(DispenserId dispenser_id)
{
    auto tx = OutLoadCellTare{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    co_await send_with_ack(server_, std::move(tx), dispenser_env(this, dispenser_id));
}

cobalt::promise<units::Litre> Pod::dispense(DispenserId dispenser_id, units::Litre volume)
{
    auto tx = OutDispense{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_millilitre().setValue(volume.numerical_value_in(units::milli_litre));

    const InDispenseFinished finish_result =
        co_await send_action_with_response<InDispenseFinished>(server_, std::move(tx), 30s, dispenser_env(this, dispenser_id));
    co_return (finish_result.field_millilitre().value() * units::milli_litre);
}

cobalt::promise<units::Litre> Pod::pump_calibrate(const DispenserId dispenser_id, const units::Steps steps)
{
    auto tx = OutPumpStartCalibration{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_pumpStep().setValue(steps);

    const InPumpFinishedCalibrationResponse finish_result = co_await send_action_with_response<InPumpFinishedCalibrationResponse>(
        server_, std::move(tx), (steps * 2ms) + 100ms, dispenser_env(this, dispenser_id));
    co_return finish_result.field_millilitre().value() * units::milli_litre;
}

cobalt::promise<void> Pod::highlight_dispenser(DispenserId dispenser_id, std::chrono::milliseconds duration)
{
    auto tx = OutHighlightDispenser{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_ledEffect().setValue(proto::field::LedEffectVal::Highlight);
    tx.field_milliseconds().setValue(duration.count());
    co_await send_with_ack(server_, std::move(tx), dispenser_env(this, dispenser_id));
}

cobalt::task<void> Pod::force_safe_state()
{
    co_await send_with_ack(server_, OutEmergencyStop{}, default_env(this));
}

cobalt::task<void> Pod::monitor_device()
{
    struct Cleanup
    {
        Pod& p_;

        ~Cleanup()
        {
            SPDLOG_LOGGER_DEBUG(p_.logger_, "monitor device loop finished.");
            p_.device_ready_ = false;
            p_.state_->update_state(PodState::ConnectionState::disconnected);
        }
    };

    Cleanup c{*this};
    state_->update_state(PodState::ConnectionState::connecting);
    const auto pod_info = co_await retry_on_timeout(
        5, [this](auto timeout) { return aquire_device_info(timeout); }, ExponentialBackoffPolicy{.maximum = 10s});
    state_->update_info(pod_info);
    SPDLOG_LOGGER_INFO(logger_, "Device '{}' is ready.", pod_info.id);
    state_->update_state(PodState::ConnectionState::connected);
    device_ready_ = true;

    co_await keep_alive();
}

cobalt::task<void> Pod::keep_alive()
{
    auto cs = co_await asio::this_coro::cancellation_state;
    co_await device_ready_;
    asio::steady_timer timer{server_.get_executor()};
    while (cs.cancelled() == asio::cancellation_type::none) {
        co_await retry_on_timeout(
            3,
            [this](auto timeout) { return send_and_receive<InPong>(server_, OutPing{}, default_env(this, timeout)); },
            ExponentialBackoffPolicy{.maximum = 1s});
        timer.expires_after(2s);
        co_await timer.async_wait(cobalt::use_op);
    }
}

DispenserEmptyError::DispenserEmptyError(PodId pod_id, DispenserId dispenser_id)
    : runtime_error{std::format("Dispenser '{}' of Pod '{}' is empty", pod_id, dispenser_id)}
    , pod_id_{pod_id}
    , dispenser_id_{dispenser_id}
{
}
} // namespace cm
