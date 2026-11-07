module;
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/system/system_error.hpp>
#include <comms/ErrorStatus.h>
#include <exec/when_any.hpp>
#include <proto/field/DispenseResultCommon.h>
#include <proto/field/ErrorCodeCommon.h>
#include <proto/field/LedEffectCommon.h>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

module cm;
import std;
import cm.core;

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
    // The version check keeps other releases out, so only a faulty Pod sends this, and guessing could pour the wrong amount.
    throw ProtocolError{comms::ErrorStatus::InvalidMsgData,
                        std::format("Unknown dispense result {}", std::to_underlying(result))};
}

DispenserPodImpl::DispenserPodImpl(std::weak_ptr<IPod> pod, PodId pod_id, DispenserId dispenser_id, std::string logger_name)
    : logger_{log::create_or_get(std::move(logger_name))}
    , pod_id_{std::move(pod_id)}
    , dispenser_id_{dispenser_id}
    , pod_{std::move(pod)}
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
        throw PodDisconnectedError{pod_id_};
    }
    return p;
}

DispenserId DispenserPodImpl::id() const
{
    return dispenser_id_;
}

Pump::Pump(std::weak_ptr<IPod> pod, PodId pod_id, DispenserId dispenser_id)
    : DispenserPodImpl{std::move(pod), std::move(pod_id), dispenser_id, std::format("pump_{}", dispenser_id)}
{
}

Valve::Valve(std::weak_ptr<IPod> pod, PodId pod_id, DispenserId dispenser_id)
    : DispenserPodImpl{std::move(pod), std::move(pod_id), dispenser_id, std::format("valve_{}", dispenser_id)}
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
    case proto::field::ErrorCodeCommon::ValueType::NoLoadCell:
    case proto::field::ErrorCodeCommon::ValueType::LoadNotSteady:
    case proto::field::ErrorCodeCommon::ValueType::TareTooOld:
    case proto::field::ErrorCodeCommon::ValueType::ImplausibleReference:
    case proto::field::ErrorCodeCommon::ValueType::ValuesLimit:
        throw PodReceiveError{env.pod_id, nak.field_errorCode().value()};
    }
    // The version check keeps other releases out, so only a faulty Pod sends a code this Station doesn't know.
    throw PodReceiveError{env.pod_id, nak.field_errorCode().value()};
}

/// Turns what a failed exchange threw into the errors of the Pod, so that the caller learns which Pod it was.
[[noreturn]] void rethrow_as_pod_error(const MessageEnvironment& env)
{
    try {
        throw;
    }
    catch (const TimeoutError&) {
        throw PodTimeoutError{env.pod_id};
    }
    catch (const boost::system::system_error& error) {
        // The session closes its channels when it ends, which is how a Pod that went away shows up.
        if (error.code() == boost::asio::experimental::error::channel_closed) {
            throw PodDisconnectedError{env.pod_id};
        }
        throw;
    }
}

template <typename Reply>
Reply reply_or_throw(std::variant<Reply, InNak> reply, const MessageEnvironment& env)
{
    if (const auto* nak = std::get_if<InNak>(&reply)) {
        process_nak(*nak, env);
    }
    return std::get<Reply>(std::move(reply));
}

/// Sends `tx_msg` and waits for `Reply`. The reply is registered before the send, so a quick one can't get lost.
template <typename Reply, typename TxMsg>
auto request(PodProtocolSession& session, TxMsg tx_msg, const MessageEnvironment env) -> Task<Reply>
{
    try {
        const auto transaction_id = session.generate_new_transaction_id();
        auto reply = session.async_receive<Reply, InNak>(transaction_id, env.timeout);
        co_await session.async_send(std::move(tx_msg), transaction_id);
        co_return reply_or_throw(co_await std::move(reply), env);
    }
    catch (...) {
        rethrow_as_pod_error(env);
    }
}

/// Like `request`, for a command that the Pod acknowledges first and finishes later with `Final`.
template <typename Final, typename TxMsg>
auto request_action(PodProtocolSession& session,
                    TxMsg tx_msg,
                    std::chrono::milliseconds action_timeout,
                    const MessageEnvironment env) -> Task<Final>
{
    try {
        const auto transaction_id = session.generate_new_transaction_id();
        // Both are registered before the send, so a final reply right behind the Ack can't get lost.
        auto ack = session.async_receive<InAck, InNak>(transaction_id, env.timeout);
        auto final_reply = session.async_receive<Final, InNak>(transaction_id, action_timeout);
        co_await session.async_send(std::move(tx_msg), transaction_id);
        std::ignore = reply_or_throw(co_await std::move(ack), env);
        co_return reply_or_throw(co_await std::move(final_reply), env);
    }
    catch (...) {
        rethrow_as_pod_error(env);
    }
}

Pod::Pod(std::unique_ptr<AnyIoStream> stream)
    : session_{std::move(stream)}
{
}

PodId Pod::pod_id() const
{
    return info_.id;
}

Task<void> Pod::run(std::unique_ptr<PodState> state)
{
    state_ = std::move(state);
    co_await exec::when_any(session_.run(), monitor_device());
}

std::expected<std::unique_ptr<Dispenser>, DispenserNotFoundError> Pod::create_dispenser(DispenserId dispenser_id)
{
    // A Pod on another protocol release hands out no dispenser, because the Station can't read its replies safely.
    if (info_.protocol_version != station_protocol_version() || dispenser_id.raw() >= (info_.num_pumps + info_.num_valves)) {
        return std::unexpected{DispenserNotFoundError{pod_id(), dispenser_id}};
    }
    if (dispenser_id.raw() >= info_.num_pumps) {
        return std::make_unique<Valve>(this->shared_from_this(), pod_id(), dispenser_id);
    }
    return std::make_unique<Pump>(this->shared_from_this(), pod_id(), dispenser_id);
}

Task<PodInfo> Pod::acquire_device_info(std::chrono::milliseconds timeout)
{
    InDeviceInfoResponse msg =
        co_await request<InDeviceInfoResponse>(session_, OutDeviceInfoRequest{}, default_env(this, timeout));
    co_return PodInfo{
        .id = PodId{msg.field_deviceName().getValue()},
        .firmware_version =
            Version{
                .major = msg.field_firmwareMajor().getValue(),
                .minor = msg.field_firmwareMinor().getValue(),
                .patch = msg.field_firmwarePatch().getValue(),
            },
        .protocol_version = protocol_version_from_wire(msg.field_protocolVersion().getValue()),
        .num_pumps = msg.field_numPumps().getValue(),
        .num_valves = msg.field_numValves().getValue(),
    };
}

Task<void> Pod::load_cell_calibrate_with_ref_weight(const DispenserId dispenser_id, const units::Grams grams)
{
    auto tx = OutLoadCellCalibrateWithRefWeight{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_gram().setValue(std::lround(grams.numerical_value_in(units::si::gram)));
    std::ignore = co_await request<InAck>(session_, std::move(tx), dispenser_env(this, dispenser_id, kLoadCellActionTimeout));
}

Task<void> Pod::load_cell_tare(DispenserId dispenser_id)
{
    auto tx = OutLoadCellTare{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    std::ignore = co_await request<InAck>(session_, std::move(tx), dispenser_env(this, dispenser_id, kLoadCellActionTimeout));
}

Task<units::Grams> Pod::load_cell_weight(const DispenserId dispenser_id)
{
    auto tx = OutLoadCellWeightRequest{};
    tx.field_dispenserId().setValue(dispenser_id.raw());

    const InLoadCellWeightResponse response = co_await request<InLoadCellWeightResponse>(
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

    const InDispenseFinished finish_result = co_await request_action<InDispenseFinished>(
        session_, std::move(tx), kDispenseFinishedTimeout, dispenser_env(this, dispenser_id, kActionAckTimeout));

    co_return DispenseReport{.amount = finish_result.field_millilitre().value() * units::milli_litre,
                             .outcome = to_dispense_outcome(finish_result.field_dispenseResult().value())};
}

Task<units::Litre> Pod::dispenser_calibrate(const DispenserId dispenser_id, const std::chrono::milliseconds run_time)
{
    auto tx = OutDispenserStartCalibration{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_milliseconds().setValue(run_time.count());

    const InDispenserFinishedCalibrationResponse finish_result = co_await request_action<InDispenserFinishedCalibrationResponse>(
        session_, std::move(tx), run_time + kCalibrationAllowance, dispenser_env(this, dispenser_id, kActionAckTimeout));
    co_return finish_result.field_millilitre().value() * units::milli_litre;
}

Task<void> Pod::highlight_dispenser(DispenserId dispenser_id, std::chrono::milliseconds duration)
{
    auto tx = OutHighlightDispenser{};
    tx.field_dispenserId().setValue(dispenser_id.raw());
    tx.field_ledEffect().setValue(proto::field::LedEffectVal::Highlight);
    tx.field_milliseconds().setValue(duration.count());
    std::ignore = co_await request<InAck>(session_, std::move(tx), dispenser_env(this, dispenser_id, kHighlightAckTimeout));
}

Task<void> Pod::force_safe_state()
{
    std::ignore = co_await request<InAck>(session_, OutEmergencyStop{}, default_env(this, kEmergencyStopAckTimeout));
}

Task<void> Pod::monitor_device()
{
    struct Cleanup
    {
        Pod& p_;

        ~Cleanup()
        {
            SPDLOG_LOGGER_DEBUG(p_.logger_, "monitor device loop finished.");
            p_.state_->update_state(PodState::ConnectionState::disconnected);
        }
    };

    Cleanup c{*this};
    state_->update_state(PodState::ConnectionState::connecting);
    PodInfo pod_info{};
    try {
        pod_info = co_await retry_on_timeout(
            5, [this](auto timeout) { return acquire_device_info(timeout); }, ExponentialBackoffPolicy{.maximum = 10s});
    }
    catch (const TimeoutError&) {
        // A Pod on another protocol release sends a device info the Station can't decode, so it looks like silence.
        SPDLOG_LOGGER_ERROR(logger_,
                            "The Pod never answered the device info request. It may speak another protocol than {}.",
                            station_protocol_version());
        throw;
    }
    info_ = pod_info;
    state_->update_info(pod_info);
    if (pod_info.protocol_version != station_protocol_version()) {
        SPDLOG_LOGGER_ERROR(logger_,
                            "Device '{}' speaks protocol {}, but the Station expects {}. It stays unused.",
                            pod_info.id,
                            pod_info.protocol_version,
                            station_protocol_version());
        state_->update_state(PodState::ConnectionState::incompatible);
        // Only a stop request ends the wait, when the Pod goes away or the Station shuts down.
        co_await wait_until_stopped(session_.get_executor());
        co_return;
    }
    SPDLOG_LOGGER_INFO(logger_, "Device '{}' is ready.", pod_info.id);
    state_->update_state(PodState::ConnectionState::connected);

    co_await keep_alive();
}

Task<void> Pod::keep_alive()
{
    // The loop only ends through a stop request, which completes the pending ping or sleep with set_stopped.
    for (;;) {
        co_await retry_on_timeout(
            3,
            [this](auto timeout) { return request<InPong>(session_, OutPing{}, default_env(this, timeout)); },
            ExponentialBackoffPolicy{.maximum = 1s});
        co_await async_sleep(session_.get_executor(), 2s);
    }
}

DispenserEmptyError::DispenserEmptyError(PodId pod_id, DispenserId dispenser_id)
    : runtime_error{std::format("Dispenser '{}' of Pod '{}' is empty", dispenser_id, pod_id)}
    , pod_id_{pod_id}
    , dispenser_id_{dispenser_id}
{
}
} // namespace cm
