module;
#include <boost/asio/steady_timer.hpp>
#include <comms/ErrorStatus.h>
#include <exec/asio/use_sender.hpp>
#include <exec/when_any.hpp>
#include <proto/field/DispenseResultCommon.h>
#include <proto/field/ErrorCodeCommon.h>
#include <proto/field/LedEffectCommon.h>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

module cm:pod_impl;
import std;
import cm.core;
import :pod;

namespace asio = boost::asio;

namespace cm {

namespace {
// These follow the reply times in protocol.adoc. Each has to be longer than the Pod's worst case.
constexpr std::chrono::milliseconds kLoadCellActionTimeout = 2s;
// The Pod reads the load cell over I2C on its protocol thread before it replies to these.
constexpr std::chrono::milliseconds kActionAckTimeout = 500ms;
constexpr std::chrono::milliseconds kLoadCellWeightTimeout = 500ms;
// This reply can wait behind such a read.
constexpr std::chrono::milliseconds kEmergencyStopAckTimeout = 500ms;
constexpr std::chrono::milliseconds kHighlightAckTimeout = 100ms;
// The Pod stops a pour after 30 s at the latest and then measures it at rest.
constexpr std::chrono::milliseconds kDispenseFinishedTimeout = 35s;
// The dispenser has to stop first, then the Pod measures at rest.
constexpr std::chrono::milliseconds kCalibrationAllowance = 3s;
} // namespace

DispenseOutcome to_dispense_outcome(const proto::field::DispenseResultVal result)
{
    using Result = proto::field::DispenseResultCommon::ValueType;
    switch (result) {
    case Result::Completed:
        return DispenseOutcome::completed;
    case Result::BottleEmpty:
        return DispenseOutcome::bottle_empty;
    case Result::TimedOut:
        return DispenseOutcome::timed_out;
    case Result::StillShort:
        return DispenseOutcome::still_short;
    case Result::ValuesLimit:
        break;
    }
    // Only a Pod on another protocol release can send this, and guessing could pour the wrong amount.
    throw ProtocolError{comms::ErrorStatus::InvalidMsgData,
                        std::format("Unknown dispense result {}", std::to_underlying(result))};
}

DispenserPodImpl::DispenserPodImpl(std::weak_ptr<IPod> pod, DispenserId dispenser_id, std::string logger_name)
    : dispenser_id_{dispenser_id}
    , pod_{std::move(pod)}
    , logger_{log::create_or_get(std::move(logger_name))}
{
}

Task<DispenseReport> DispenserPodImpl::dispense(units::Litre volume)
{
    SPDLOG_LOGGER_DEBUG(logger_, "Start dispense of {}.", volume);
    const auto report = co_await pod()->dispense(dispenser_id_, volume);
    SPDLOG_LOGGER_DEBUG(logger_, "Finished dispensing of {}. Dispensed: {}.", volume, report.amount);
    co_return report;
}

Task<units::Litre> DispenserPodImpl::calibrate(std::chrono::milliseconds run_time)
{
    SPDLOG_LOGGER_DEBUG(logger_, "Start calibration with a run time of {}.", run_time);
    co_return co_await pod()->dispenser_calibrate(id(), run_time);
}

Task<void> DispenserPodImpl::load_cell_calibrate_with_ref_weight(units::Grams grams)
{
    SPDLOG_LOGGER_DEBUG(logger_, "Set load cell ref weight to {}.", grams);
    co_await pod()->load_cell_calibrate_with_ref_weight(dispenser_id_, grams);
}

Task<void> DispenserPodImpl::load_cell_tare()
{
    SPDLOG_LOGGER_DEBUG(logger_, "load cell TARE.");
    co_await pod()->load_cell_tare(dispenser_id_);
}

Task<units::Grams> DispenserPodImpl::load_cell_weight()
{
    co_return co_await pod()->load_cell_weight(dispenser_id_);
}

Task<void> DispenserPodImpl::highlight(std::chrono::milliseconds duration)
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

MessageEnvironment dispenser_env(Pod* pod, DispenserId dispenser_id, std::chrono::milliseconds timeout)
{
    return MessageEnvironment{.pod_id = pod->pod_id(), .dispenser_id = dispenser_id, .timeout = timeout};
}

MessageEnvironment default_env(Pod* pod, std::chrono::milliseconds timeout)
{
    return MessageEnvironment{.pod_id = pod->pod_id(), .timeout = timeout};
}

void process_nak(const InNak& nak, const MessageEnvironment& env)
{
    switch (nak.field_errorCode().value()) {
    case proto::field::ErrorCodeCommon::ValueType::DispenserEmpty:
        throw DispenserEmptyError{env.pod_id, env.dispenser_id};
    case proto::field::ErrorCodeCommon::ValueType::DispenserNotFound:
    case proto::field::ErrorCodeCommon::ValueType::InvalidParameter:
    case proto::field::ErrorCodeCommon::ValueType::UnsupportedInCurrentState:
    case proto::field::ErrorCodeCommon::ValueType::HardwareFault:
    case proto::field::ErrorCodeCommon::ValueType::Busy:
    case proto::field::ErrorCodeCommon::ValueType::NotCalibrated:
    case proto::field::ErrorCodeCommon::ValueType::InternalError:
    case proto::field::ErrorCodeCommon::ValueType::ValuesLimit:
        throw PodReceiveError{env.pod_id, nak.field_errorCode().value()};
    }
    // A Pod on a newer protocol can send a code this Station doesn't know.
    throw PodReceiveError{env.pod_id, nak.field_errorCode().value()};
}

template <typename TxMsg>
auto send_with_ack(PodProtocolSession& session, TxMsg tx_msg, const MessageEnvironment env) -> Task<void>
{
    const auto transaction_id = session.generate_new_transaction_id();

    // register queue at first
    auto rx_action = session.async_receive<InAck, InNak>(transaction_id, env.timeout);

    co_await session.async_send(std::move(tx_msg), transaction_id);
    try {
        const auto nak_or_ack = co_await std::move(rx_action);
        if (std::holds_alternative<InNak>(nak_or_ack)) {
            process_nak(std::get<InNak>(nak_or_ack), env);
        }
    }
    catch (const TimeoutError&) {
        throw PodTimeoutError{env.pod_id};
    }
}

template <typename RxMsg, typename TxMsg>
auto send_and_receive(PodProtocolSession& session, TxMsg tx_msg, const MessageEnvironment env) -> Task<RxMsg>
{
    const auto transaction_id = session.generate_new_transaction_id();

    // register queue at first
    auto rx_action = session.async_receive<RxMsg>(transaction_id, env.timeout);

    co_await session.async_send(std::move(tx_msg), transaction_id);
    try {
        co_return co_await std::move(rx_action);
    }
    catch (const TimeoutError&) {
        throw PodTimeoutError{env.pod_id};
    }
}

/// Like `send_and_receive`, but for a request the Pod can answer with a Nak.
template <typename RxMsg, typename TxMsg>
auto send_and_receive_or_nak(PodProtocolSession& session, TxMsg tx_msg, const MessageEnvironment env) -> Task<RxMsg>
{
    const auto transaction_id = session.generate_new_transaction_id();

    // register queue at first
    auto rx_action = session.async_receive<RxMsg, InNak>(transaction_id, env.timeout);

    co_await session.async_send(std::move(tx_msg), transaction_id);
    try {
        const auto nak_or_response = co_await std::move(rx_action);
        if (std::holds_alternative<InNak>(nak_or_response)) {
            process_nak(std::get<InNak>(nak_or_response), env);
        }
        co_return std::get<RxMsg>(nak_or_response);
    }
    catch (const TimeoutError&) {
        throw PodTimeoutError{env.pod_id};
    }
}

template <typename RxMsg, typename TxMsg>
auto send_action_with_response(PodProtocolSession& session,
                               TxMsg tx_msg,
                               std::chrono::milliseconds action_timeout,
                               const MessageEnvironment env) -> Task<RxMsg>
{
    const auto transaction_id = session.generate_new_transaction_id();

    // Both are registered before the send, so a final reply right behind the Ack can't get lost.
    auto ack_action = session.async_receive<InAck, InNak>(transaction_id, env.timeout);
    auto final_action = session.async_receive<RxMsg, InNak>(transaction_id, action_timeout);

    co_await session.async_send(std::move(tx_msg), transaction_id);
    try {
        const auto nak_or_ack = co_await std::move(ack_action);
        if (std::holds_alternative<InNak>(nak_or_ack)) {
            process_nak(std::get<InNak>(nak_or_ack), env);
        }

        const auto nak_or_action = co_await std::move(final_action);
        if (std::holds_alternative<InNak>(nak_or_action)) {
            process_nak(std::get<InNak>(nak_or_action), env);
        }
        co_return std::get<RxMsg>(nak_or_action);
    }
    catch (const TimeoutError&) {
        throw PodTimeoutError{env.pod_id};
    }
}

Pod::Pod(std::unique_ptr<AnyIoStream> stream)
    : session_{std::move(stream)}
    , device_ready_{session_.get_executor()}
{
}

PodId Pod::pod_id() const
{
    return state_->info().id;
}

Task<void> Pod::run(std::unique_ptr<PodState> state)
{
    state_ = std::move(state);
    co_await exec::when_any(session_.run(), monitor_device());
}

std::expected<std::unique_ptr<Dispenser>, DispenserNotFoundError> Pod::create_dispenser(DispenserId dispenser_id)
{
    const auto info = state_->info();
    if (dispenser_id.raw() >= (info.num_pumps + info.num_valves)) {
        return std::unexpected{DispenserNotFoundError{pod_id(), dispenser_id}};
    }
    if (dispenser_id.raw() >= info.num_pumps) {
        return std::make_unique<Valve>(this->shared_from_this(), dispenser_id);
    }
    return std::make_unique<Pump>(this->shared_from_this(), dispenser_id);
}

Task<PodInfo> Pod::aquire_device_info(std::chrono::milliseconds timeout)
{
    InDeviceInfoResponse msg =
        co_await send_and_receive<InDeviceInfoResponse>(session_, OutDeviceInfoRequest{}, default_env(this, timeout));
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

Task<void> Pod::load_cell_calibrate_with_ref_weight(const DispenserId dispenser_id, const units::Grams grams)
{
    auto tx = OutLoadCellCalibrateWithRefWeight{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_gram().setValue(std::lround(grams.numerical_value_in(units::si::gram)));
    co_await send_with_ack(session_, std::move(tx), dispenser_env(this, dispenser_id, kLoadCellActionTimeout));
}

Task<void> Pod::load_cell_tare(DispenserId dispenser_id)
{
    auto tx = OutLoadCellTare{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    co_await send_with_ack(session_, std::move(tx), dispenser_env(this, dispenser_id, kLoadCellActionTimeout));
}

Task<units::Grams> Pod::load_cell_weight(const DispenserId dispenser_id)
{
    auto tx = OutLoadCellWeightRequest{};
    tx.field_dispenserId().setValue(dispenser_id.raw());

    const InLoadCellWeightResponse response = co_await send_and_receive_or_nak<InLoadCellWeightResponse>(
        session_, std::move(tx), dispenser_env(this, dispenser_id, kLoadCellWeightTimeout));
    co_return response.field_gram().value() * units::si::gram;
}

Task<DispenseReport> Pod::dispense(DispenserId dispenser_id, units::Litre volume)
{
    const auto millilitres = std::lround(volume.numerical_value_in(units::milli_litre));
    // The Pod refuses an amount of 0 mL or less, and there is nothing to pour anyway.
    if (millilitres <= 0) {
        co_return DispenseReport{.amount = 0.0 * units::milli_litre, .outcome = DispenseOutcome::completed};
    }

    auto tx = OutDispense{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_millilitre().setValue(millilitres);

    const InDispenseFinished finish_result = co_await send_action_with_response<InDispenseFinished>(
        session_, std::move(tx), kDispenseFinishedTimeout, dispenser_env(this, dispenser_id, kActionAckTimeout));

    co_return DispenseReport{.amount = finish_result.field_millilitre().value() * units::milli_litre,
                             .outcome = to_dispense_outcome(finish_result.field_dispenseResult().value())};
}

Task<units::Litre> Pod::dispenser_calibrate(const DispenserId dispenser_id, const std::chrono::milliseconds run_time)
{
    auto tx = OutDispenserStartCalibration{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_milliseconds().setValue(run_time.count());

    const InDispenserFinishedCalibrationResponse finish_result =
        co_await send_action_with_response<InDispenserFinishedCalibrationResponse>(
            session_, std::move(tx), run_time + kCalibrationAllowance, dispenser_env(this, dispenser_id, kActionAckTimeout));
    co_return finish_result.field_millilitre().value() * units::milli_litre;
}

Task<void> Pod::highlight_dispenser(DispenserId dispenser_id, std::chrono::milliseconds duration)
{
    auto tx = OutHighlightDispenser{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_ledEffect().setValue(proto::field::LedEffectVal::Highlight);
    tx.field_milliseconds().setValue(duration.count());
    co_await send_with_ack(session_, std::move(tx), dispenser_env(this, dispenser_id, kHighlightAckTimeout));
}

Task<void> Pod::force_safe_state()
{
    co_await send_with_ack(session_, OutEmergencyStop{}, default_env(this, kEmergencyStopAckTimeout));
}

Task<void> Pod::monitor_device()
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

Task<void> Pod::keep_alive()
{
    co_await device_ready_.async_wait();
    asio::steady_timer timer{session_.get_executor()};
    // The loop only ends through a stop request, which completes the pending ping or timer wait with set_stopped.
    for (;;) {
        co_await retry_on_timeout(
            3,
            [this](auto timeout) { return send_and_receive<InPong>(session_, OutPing{}, default_env(this, timeout)); },
            ExponentialBackoffPolicy{.maximum = 1s});
        timer.expires_after(2s);
        co_await timer.async_wait(exec::asio::use_sender);
    }
}

DispenserEmptyError::DispenserEmptyError(PodId pod_id, DispenserId dispenser_id)
    : runtime_error{std::format("Dispenser '{}' of Pod '{}' is empty", dispenser_id, pod_id)}
    , pod_id_{pod_id}
    , dispenser_id_{dispenser_id}
{
}
} // namespace cm
