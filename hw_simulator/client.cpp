module;
#include <boost/asio.hpp>
#include <comms/process.h>
#include <exec/asio/use_sender.hpp>
#include <exec/when_any.hpp>
#include <proto/FrameInterface.h>
#include <proto/dispatch/DispatchClientInputMessage.h>
#include <proto/frame/Frame.h>
#include <proto/input/AllMessages.h>
#include <proto/input/ClientInputMessages.h>
#include <proto/options/ClientDefaultOptions.h>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

module cm.sim;
import std;
import mp_units;
import cm.core;
import cm;

using namespace std::chrono_literals;
namespace asio = boost::asio;

namespace cm::sim {
Client::Client(AsyncScope& scope, std::string name, Version firmware_version, ClientConfig config)
    : logger_{log::create_or_get(std::format("SimPod_{}", name))}
    , scope_{scope}
    , stream_{scope.executor()}
    , write_queue_{scope.executor()}
    , config_{std::move(config)}
{
    power_cycle_load_cells();
    device_info_response_msg_.field_protocolVersion().setValue(protocol_version_to_wire(config_.protocol_version));
    device_info_response_msg_.field_deviceName().setValue(std::move(name));
    device_info_response_msg_.field_firmwareMajor().setValue(firmware_version.major);
    device_info_response_msg_.field_firmwareMinor().setValue(firmware_version.minor);
    device_info_response_msg_.field_firmwarePatch().setValue(firmware_version.patch);
    device_info_response_msg_.field_numPumps().setValue(kNumPumps);
    device_info_response_msg_.field_numValves().setValue(kNumValves);
}

void Client::power_cycle_load_cells()
{
    for (auto& state : dispensers_) {
        state.zero = load_of(state);
    }
}

Task<void> Client::run()
{
    try {
        co_await exec::when_any(read_loop(), write_loop());
    }
    catch (const boost::system::system_error& e) {
        SPDLOG_LOGGER_ERROR(logger_, "I/O loops terminated: {}", e.what());
    }
}

void Client::handle(Message& msg)
{
    SPDLOG_LOGGER_WARN(logger_, "Got unexpected message '{}'", msg.name());
}

Task<void> Client::async_handle(std::shared_ptr<Client> /*self*/, InClientDeviceInfoRequest msg)
{
    co_await delay(config_.quick_reply);
    co_await async_send(device_info_response_msg_, msg.transportField_transactionId().getValue());
}

Task<void> Client::async_handle(std::shared_ptr<Client> /*self*/, InClientPing msg)
{
    co_await delay(config_.quick_reply);
    co_await async_send(Pong{}, msg.transportField_transactionId().getValue());
}

Task<void> Client::async_handle(std::shared_ptr<Client> /*self*/, InClientEmergencyStop msg)
{
    ++stats_.emergency_stops;

    // A tare or a load cell calibration carries on and replies by itself.
    std::vector<TransactionId::ValueType> interrupted;
    for (auto& state : dispensers_) {
        if (state.activity == Activity::pour || state.activity == Activity::calibration) {
            interrupted.push_back(state.transaction_id);
            state.activity = Activity::idle;
            ++state.generation;
        }
    }

    co_await delay(config_.quick_reply);
    co_await async_send(Ack{}, msg.transportField_transactionId().getValue());
    for (const auto transaction_id : interrupted) {
        co_await delay(config_.stop_nak_spacing);
        co_await send_nak(transaction_id, ErrorCode::HardwareFault);
    }
}

Task<void> Client::async_handle(std::shared_ptr<Client> /*self*/, InClientLoadCellWeightRequest msg)
{
    const auto trid = msg.transportField_transactionId().getValue();
    const auto id = msg.field_dispenserId().value();
    ++stats_.weight_requests;
    ++outstanding_weight_requests_;
    stats_.max_outstanding_weight_requests = std::max(stats_.max_outstanding_weight_requests, outstanding_weight_requests_);

    co_await delay(config_.quick_reply);
    --outstanding_weight_requests_;
    if (const auto refused = load_cell_refusal(id)) {
        co_await send_nak(trid, *refused);
        co_return;
    }

    const auto& state = dispensers_.at(id);
    WeightResponse response{};
    response.field_gram().setValue(std::lround((load_of(state) - state.zero).numerical_value_in(units::si::gram)));
    co_await async_send(std::move(response), trid);
}

Task<void> Client::async_handle(std::shared_ptr<Client> /*self*/, InClientLoadCellTare msg)
{
    const auto trid = msg.transportField_transactionId().getValue();
    const auto id = msg.field_dispenserId().value();
    ++stats_.tare_requests;
    if (const auto refused = start_refusal(id)) {
        co_await delay(config_.quick_reply);
        co_await send_nak(trid, *refused);
        co_return;
    }

    auto& state = dispensers_.at(id);
    state.activity = Activity::load_cell;
    co_await delay(config_.load_cell_action);
    state.zero = load_of(state);
    state.last_tare = Clock::now();
    state.activity = Activity::idle;
    co_await async_send(Ack{}, trid);
}

Task<void> Client::async_handle(std::shared_ptr<Client> /*self*/, InClientLoadCellCalibrateWithRefWeight msg)
{
    const auto trid = msg.transportField_transactionId().getValue();
    const auto id = msg.field_dispenserId().value();
    auto refused = start_refusal(id);
    if (!refused.has_value() && msg.field_gram().value() <= 0) {
        refused = ErrorCode::InvalidParameter;
    }
    if (!refused.has_value() && !has_recent_tare(dispensers_.at(id))) {
        refused = ErrorCode::TareTooOld;
    }
    if (refused.has_value()) {
        co_await delay(config_.quick_reply);
        co_await send_nak(trid, *refused);
        co_return;
    }

    auto& state = dispensers_.at(id);
    state.activity = Activity::load_cell;
    co_await delay(config_.load_cell_action);
    state.last_tare.reset();
    state.activity = Activity::idle;
    co_await async_send(Ack{}, trid);
}

Task<void> Client::async_handle(std::shared_ptr<Client> self, InClientDispenserStartCalibration msg)
{
    const auto trid = msg.transportField_transactionId().getValue();
    const auto id = msg.field_dispenserId().value();
    const std::chrono::milliseconds run_time{msg.field_milliseconds().value()};

    std::optional<ErrorCode> refused;
    if (!is_valid_dispenser(id)) {
        refused = ErrorCode::DispenserNotFound;
    }
    else if (run_time <= 0ms || run_time > config_.longest_run) {
        refused = ErrorCode::InvalidParameter;
    }
    else {
        refused = start_refusal(id);
    }
    if (refused.has_value()) {
        co_await delay(config_.ack_delay);
        co_await send_nak(trid, *refused);
        co_return;
    }

    auto& state = dispensers_.at(id);
    const auto generation = begin_action(state, Activity::calibration, trid);
    co_await delay(config_.ack_delay);
    if (state.generation != generation) {
        co_return;
    }
    co_await async_send(Ack{}, trid);

    const auto result = co_await run_dispenser(state, generation, std::nullopt, run_time);
    if (!result.has_value()) {
        co_return;
    }

    state.activity = Activity::idle;
    if (result->stalled || result->amount < config_.min_calibration_amount) {
        co_await send_nak(trid, ErrorCode::DispenserEmpty);
    }
    else {
        CalibrationFinished response{};
        response.field_millilitre().setValue(std::lround(result->amount.numerical_value_in(units::si::gram)));
        co_await async_send(std::move(response), trid);
    }
    refill_if_dry(self, id);
}

Task<void> Client::async_handle(std::shared_ptr<Client> /*self*/, InClientHighlightDispenser msg)
{
    const auto trid = msg.transportField_transactionId().getValue();
    co_await delay(config_.quick_reply);
    if (is_valid_dispenser(msg.field_dispenserId().value())) {
        co_await async_send(Ack{}, trid);
    }
    else {
        co_await send_nak(trid, ErrorCode::DispenserNotFound);
    }
}

Task<void> Client::async_handle(std::shared_ptr<Client> self, InClientDispense msg)
{
    const auto trid = msg.transportField_transactionId().getValue();
    const auto id = msg.field_dispenserId().value();
    const int requested = msg.field_millilitre().value();
    ++stats_.dispense_requests;
    stats_.last_dispense_ml = requested;

    std::optional<ErrorCode> refused;
    if (!is_valid_dispenser(id)) {
        refused = ErrorCode::DispenserNotFound;
    }
    else if (requested <= 0) {
        refused = ErrorCode::InvalidParameter;
    }
    else {
        refused = start_refusal(id);
    }
    if (refused.has_value()) {
        co_await delay(config_.ack_delay);
        co_await send_nak(trid, *refused);
        co_return;
    }

    auto& state = dispensers_.at(id);
    const auto generation = begin_action(state, Activity::pour, trid);
    co_await delay(config_.ack_delay);
    if (state.generation != generation) {
        co_return;
    }
    co_await async_send(Ack{}, trid);

    const units::Grams target =
        std::max(requested * units::si::gram - config_.trim_shortfall.value_or(units::Grams{}), 0.0 * units::si::gram);
    const auto result = co_await run_dispenser(state, generation, target, config_.pour_limit);
    if (!result.has_value()) {
        co_return;
    }

    // An empty bottle isn't an error. The result tells the Station why the pour came up short.
    state.activity = Activity::idle;
    DispenseFinished finished{};
    finished.field_millilitre().setValue(std::lround(result->amount.numerical_value_in(units::si::gram)));
    if (result->stalled) {
        finished.field_dispenseResult().setValue(DispenseResult::BottleEmpty);
    }
    else if (not result->reached_target) {
        finished.field_dispenseResult().setValue(DispenseResult::TimedOut);
    }
    else if (config_.trim_shortfall.has_value()) {
        finished.field_dispenseResult().setValue(DispenseResult::StillShort);
    }
    else {
        finished.field_dispenseResult().setValue(DispenseResult::Completed);
    }
    co_await async_send(std::move(finished), trid);
    refill_if_dry(self, id);
}

Task<void> Client::async_refill(std::shared_ptr<Client> /*self*/, DispenserState& state)
{
    co_await delay(*config_.auto_refill_after);
    // The bottle may be in use or filled again by now.
    if (state.activity == Activity::idle && state.bottle.liquid <= 0.0 * units::si::gram) {
        state.bottle.liquid = config_.refill_to;
    }
}

auto Client::run_dispenser(DispenserState& state,
                           const std::uint64_t generation,
                           const std::optional<units::Grams> target,
                           const std::chrono::milliseconds time_limit) -> Task<std::optional<RunResult>>
{
    const auto start = Clock::now();
    const auto end_of_run = start + time_limit;
    auto last_tick = start;
    auto last_progress = start;
    auto last_report = start;
    units::Grams amount = 0 * units::si::gram;
    bool stalled = false;
    bool reached_target = false;

    while (Clock::now() < end_of_run) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end_of_run - Clock::now());
        co_await delay(std::clamp<std::chrono::milliseconds>(left, 1ms, kTick));
        if (state.generation != generation) {
            co_return std::nullopt;
        }

        // Timers run late on a busy machine, so the amount has to follow the clock and not the number of ticks.
        const auto now = Clock::now();
        const auto flow_until = std::min(now, end_of_run);
        auto flowed = state.bottle.hose_blocked ? 0.0 * units::si::gram
                                                : std::min(flow_during(flow_until - last_tick), state.bottle.liquid);
        last_tick = flow_until;
        if (target.has_value()) {
            flowed = std::min(flowed, *target - amount);
        }
        state.bottle.liquid -= flowed;
        amount += flowed;

        if (flowed > 0.0 * units::si::gram) {
            last_progress = now;
        }
        if (target.has_value() && amount >= *target) {
            reached_target = true;
            break;
        }
        if (now - last_progress >= config_.stall) {
            stalled = true;
            break;
        }
        if (now - last_report >= config_.progress_interval) {
            last_report = now;
            co_await send_progress(state, amount);
        }
    }

    co_await delay(config_.settle);
    if (state.generation != generation) {
        co_return std::nullopt;
    }
    co_return RunResult{.amount = amount, .stalled = stalled, .reached_target = reached_target};
}

Task<void> Client::send_progress(const DispenserState& state, const units::Grams amount)
{
    const auto millilitre = static_cast<int>(std::lround(amount.numerical_value_in(units::si::gram)));
    ++stats_.progress_reports;
    stats_.last_progress_ml = millilitre;
    DispenseProgress progress{};
    progress.field_millilitre().setValue(millilitre);
    co_await async_send(std::move(progress), state.transaction_id);
}

Task<void> Client::send_nak(const TransactionId::ValueType transaction_id, const ErrorCode error)
{
    Nak nak{};
    nak.field_errorCode().setValue(error);
    co_await async_send(std::move(nak), transaction_id);
}

std::uint64_t Client::begin_action(DispenserState& state, const Activity activity, const TransactionId::ValueType transaction_id)
{
    state.activity = activity;
    state.transaction_id = transaction_id;
    return ++state.generation;
}

void Client::refill_if_dry(const std::shared_ptr<Client>& self, const int id)
{
    auto& state = dispensers_.at(id);
    if (config_.auto_refill_after.has_value() && state.bottle.liquid <= 0.0 * units::si::gram) {
        scope_.spawn(async_refill(self, state));
    }
}

bool Client::is_valid_dispenser(const int id) const
{
    return id >= 0 && static_cast<std::size_t>(id) < dispensers_.size();
}

bool Client::has_load_cell(const int id) const
{
    return config_.without_load_cell != DispenserId{id};
}

auto Client::load_cell_refusal(const int id) const -> std::optional<ErrorCode>
{
    if (!is_valid_dispenser(id)) {
        return ErrorCode::DispenserNotFound;
    }
    if (!has_load_cell(id)) {
        return ErrorCode::NoLoadCell;
    }
    return std::nullopt;
}

auto Client::start_refusal(const int id) const -> std::optional<ErrorCode>
{
    if (const auto refused = load_cell_refusal(id)) {
        return refused;
    }
    if (dispensers_.at(id).activity != Activity::idle) {
        return ErrorCode::Busy;
    }
    return std::nullopt;
}

bool Client::has_recent_tare(const DispenserState& state) const
{
    return state.last_tare.has_value() && Clock::now() - *state.last_tare < config_.tare_valid_for;
}

units::Grams Client::load_of(const DispenserState& state)
{
    return state.bottle.on_scale ? state.bottle.glass + state.bottle.liquid : 0.0 * units::si::gram;
}

units::Grams Client::flow_during(const std::chrono::duration<double> duration) const
{
    const double millilitres_per_second = config_.flow.numerical_value_in(units::milli_litre / units::si::second);
    return millilitres_per_second * duration.count() * units::si::gram;
}

Task<void> Client::delay(std::chrono::milliseconds duration)
{
    co_await async_sleep(stream_.get_executor(), duration);
}

Task<void> Client::async_send(auto msg, const TransactionId::ValueType transaction_id)
{
    SPDLOG_LOGGER_TRACE(logger_, "Schedule send {} with transaction id '{}'", msg.name(), transaction_id);
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
    co_await cm::async_send(write_queue_, std::move(output));
}

Task<void> Client::write_loop()
{
    while (true) {
        std::vector<uint8_t> data;
        try {
            data = co_await cm::async_receive(write_queue_);
        }
        catch (...) {
            break;
        }

        auto [ec, bytes_written] = co_await asio::async_write(
            stream_,
            asio::buffer(data),
            asio::cancel_after(std::chrono::milliseconds(500), asio::as_tuple(exec::asio::use_sender)));
        if (ec == asio::error::operation_aborted && (co_await stdexec::read_env(stdexec::get_stop_token)).stop_requested()) {
            co_yield stdexec::with_stopped{};
        }
        if (ec) {
            SPDLOG_LOGGER_ERROR(logger_, "Could not write to the Station, ending the session: {}", ec.message());
            break;
        }
    }
}

Task<void> Client::read_loop()
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
                SPDLOG_LOGGER_ERROR(logger_, "Fatal: Buffer limit exceeded. Dropping connection to prevent OOM.");
                co_return;
            }
            rx_buffer.resize(rx_buffer.size() * 2);
        }

        auto [ec, bytes_read] = co_await stream_.async_read_some(
            asio::buffer(rx_buffer.data() + valid_bytes, rx_buffer.size() - valid_bytes), asio::as_tuple(exec::asio::use_sender));

        if (ec) {
            SPDLOG_LOGGER_DEBUG(logger_, "Could not read from stream. Returning from read-loop. Reason: {}", ec.message());
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

std::unique_ptr<AnyIoStream> open_station_stream(Client& client)
{
    Socket station_socket{client.socket().get_executor()};
    asio::local::connect_pair(client.socket(), station_socket);
    return std::make_unique<SocketIoStream<Socket>>(std::move(station_socket));
}
} // namespace cm::sim
