module;
#include <boost/asio.hpp>
#include <proto/FrameInterface.h>
#include <proto/frame/Frame.h>
#include <proto/input/AllMessages.h>
#include <proto/input/ClientInputMessages.h>
#include <proto/options/ClientDefaultOptions.h>

export module cm.sim:client;
import std;
import mp_units;
import cm.core;
import cm;

using namespace std::chrono_literals;
namespace asio = boost::asio;

namespace cm::sim {
export using Socket = asio::local::stream_protocol::socket;

/// How the simulated pod behaves. The defaults follow the real Pod, and tests shorten what would make them slow.
export struct ClientConfig
{
    std::chrono::milliseconds load_cell_action{1300ms};
    std::chrono::milliseconds ack_delay{50ms};
    std::chrono::milliseconds quick_reply{10ms};
    std::chrono::milliseconds settle{1500ms};
    std::chrono::milliseconds stall{5s};
    std::chrono::milliseconds pour_limit{30s};
    std::chrono::milliseconds longest_run{30s};
    std::chrono::milliseconds stop_nak_spacing{20ms};
    std::chrono::milliseconds tare_valid_for{5min};
    units::Grams min_calibration_amount{5 * units::si::gram};
    units::LitrePerSecond flow{4.0 * units::milli_litre / units::si::second};
    std::optional<DispenserId> without_load_cell{DispenserId{4}};
    std::optional<std::chrono::milliseconds> auto_refill_after{};
    units::Grams refill_to{500 * units::si::gram};
    /// Makes every pour stop this far short and report that its trims are used up.
    std::optional<units::Grams> trim_shortfall{};
    Version protocol_version{station_protocol_version()};
    /// The firmware reports the amount so far about four times a second while a dispenser runs.
    std::chrono::milliseconds progress_interval{250ms};
};

/// Quick timings for tests, so that the real procedures still finish fast.
export inline ClientConfig fast_test_config()
{
    return ClientConfig{
        .load_cell_action = 50ms,
        .settle = 50ms,
        .stall = 50ms,
        .flow = 1000.0 * units::milli_litre / units::si::second,
    };
}

/// A bottle on a scale. Tests change it directly, as nothing else reaches the simulated pod but protocol messages.
export struct SimBottle
{
    units::Grams glass{500 * units::si::gram};
    units::Grams liquid{500 * units::si::gram};
    bool on_scale{true};
    bool hose_blocked{false};
};

export struct ClientStats
{
    int tare_requests{};
    int dispense_requests{};
    int last_dispense_ml{};
    int weight_requests{};
    int max_outstanding_weight_requests{};
    int emergency_stops{};
    int progress_reports{};
    int last_progress_ml{};
};

/**
 * A simulated pod that answers the Station's protocol requests over a `Socket`.
 *
 * It must be owned by a `std::shared_ptr`, because every request is handled by a spawned task that keeps its client alive
 * until it is done.
 */
export class Client : public std::enable_shared_from_this<Client>
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
    using CalibrationFinished = proto::message::DispenserFinishedCalibrationResponse<Message, Options>;
    using DispenseFinished = proto::message::DispenseFinished<Message, Options>;
    using DispenseProgress = proto::message::DispenseProgress<Message, Options>;
    using WeightResponse = proto::message::LoadCellWeightResponse<Message, Options>;
    using ErrorCode = proto::field::ErrorCodeCommon::ValueType;
    using DispenseResult = proto::field::DispenseResultCommon::ValueType;
    using Clock = std::chrono::steady_clock;

    static constexpr int kNumPumps = 5;
    static constexpr int kNumValves = 3;
    static constexpr auto kTick = 100ms;

    /// The requests that get an answer. The dispatcher hands every other message to `handle(Message&)`.
    template <typename TMessage>
    static constexpr bool kAnswered =
        std::same_as<TMessage, InClientDeviceInfoRequest> || std::same_as<TMessage, InClientPing> ||
        std::same_as<TMessage, InClientLoadCellCalibrateWithRefWeight> || std::same_as<TMessage, InClientLoadCellTare> ||
        std::same_as<TMessage, InClientLoadCellWeightRequest> || std::same_as<TMessage, InClientDispenserStartCalibration> ||
        std::same_as<TMessage, InClientHighlightDispenser> || std::same_as<TMessage, InClientDispense> ||
        std::same_as<TMessage, InClientEmergencyStop>;

    enum class Activity
    {
        idle,
        load_cell,
        pour,
        calibration,
    };

    struct DispenserState
    {
        SimBottle bottle;
        units::Grams zero{};
        Activity activity{Activity::idle};
        // Tells a running action that an emergency stop has ended it.
        std::uint64_t generation{};
        TransactionId::ValueType transaction_id{};
        std::optional<Clock::time_point> last_tare;
    };

    struct RunResult
    {
        units::Grams amount;
        bool stalled;
        bool reached_target;
    };

    log::Logger logger_;
    AsyncScope& scope_;
    Socket stream_;
    DeviceInfoResponse device_info_response_msg_{};
    Channel<std::vector<uint8_t>> write_queue_;
    ClientFrame frame_;
    ClientConfig config_;
    ClientStats stats_;
    int outstanding_weight_requests_{};
    std::array<DispenserState, (kNumPumps + kNumValves)> dispensers_;

  public:
    Client(AsyncScope& scope, std::string name, Version firmware_version, ClientConfig config = {});

    Socket& socket()
    {
        return stream_;
    }

    SimBottle& bottle(DispenserId id)
    {
        return dispensers_.at(id.raw()).bottle;
    }

    /// Every load cell takes its current load as the zero point, as it does at each power up.
    void power_cycle_load_cells();

    const ClientStats& stats() const
    {
        return stats_;
    }

    Task<void> run();

    template <typename TMessage>
        requires kAnswered<TMessage>
    void handle(TMessage& msg)
    {
        scope_.spawn(async_handle(shared_from_this(), msg));
    }

    void handle(Message& msg);

  private:
    Task<void> async_handle(std::shared_ptr<Client> self, InClientDeviceInfoRequest msg);
    Task<void> async_handle(std::shared_ptr<Client> self, InClientPing msg);
    Task<void> async_handle(std::shared_ptr<Client> self, InClientEmergencyStop msg);
    Task<void> async_handle(std::shared_ptr<Client> self, InClientLoadCellWeightRequest msg);
    Task<void> async_handle(std::shared_ptr<Client> self, InClientLoadCellTare msg);
    Task<void> async_handle(std::shared_ptr<Client> self, InClientLoadCellCalibrateWithRefWeight msg);
    Task<void> async_handle(std::shared_ptr<Client> self, InClientDispenserStartCalibration msg);
    Task<void> async_handle(std::shared_ptr<Client> self, InClientHighlightDispenser msg);
    Task<void> async_handle(std::shared_ptr<Client> self, InClientDispense msg);
    Task<void> async_refill(std::shared_ptr<Client> self, DispenserState& state);

    /**
     * Runs a dispenser until `target` came out, the time limit passed or the flow stalled, then lets the load settle.
     *
     * @returns Nothing if an emergency stop ended the run, which then replies for it.
     */
    Task<std::optional<RunResult>> run_dispenser(DispenserState& state,
                                                 std::uint64_t generation,
                                                 std::optional<units::Grams> target,
                                                 std::chrono::milliseconds time_limit);

    Task<void> send_progress(const DispenserState& state, units::Grams amount);
    Task<void> send_nak(TransactionId::ValueType transaction_id, ErrorCode error);
    std::uint64_t begin_action(DispenserState& state, Activity activity, TransactionId::ValueType transaction_id);
    void refill_if_dry(const std::shared_ptr<Client>& self, int id);
    bool is_valid_dispenser(int id) const;
    bool has_load_cell(int id) const;
    std::optional<ErrorCode> load_cell_refusal(int id) const;
    std::optional<ErrorCode> start_refusal(int id) const;
    bool has_recent_tare(const DispenserState& state) const;
    static units::Grams load_of(const DispenserState& state);
    units::Grams flow_during(std::chrono::duration<double> duration) const;

    Task<void> delay(std::chrono::milliseconds duration = std::chrono::milliseconds{50});
    Task<void> async_send(auto msg, TransactionId::ValueType transaction_id);
    Task<void> write_loop();
    Task<void> read_loop();
};

/// Connects `client` to a new socket, as a USB port would, and returns the Station's end of it.
export std::unique_ptr<AnyIoStream> open_station_stream(Client& client);
} // namespace cm::sim
