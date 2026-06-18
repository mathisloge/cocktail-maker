module;
#include <boost/asio.hpp>
#include <boost/cobalt.hpp>

export module cm:async_machine_interface;

import std;
import :logging;
import :units;
import :strong_type;
import :async_machine_protocol_server;
import :awaitable_bool;
import :overloaded;
import :ingredient;

namespace cm {

namespace cobalt = boost::cobalt;
namespace asio = boost::asio;

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

export using PumpId = strong_type<int, struct PumpIdTag, Comparable, Hashable, Formattable>;
export using ValveId = strong_type<int, struct ValveIdTag, Comparable, Hashable, Formattable>;

export class DispenserNotFound : public std::runtime_error
{
    std::variant<IngredientId, PumpId> id_;

  public:
    DispenserNotFound(IngredientId ingredient_id)
        : std::runtime_error{std::format("Could not find a dispenser for ingredient_id '{}'", ingredient_id)}
        , id_{ingredient_id}
    {
    }

    DispenserNotFound(PumpId pump_id)
        : std::runtime_error{std::format("Could not find a pump with id '{}'", pump_id)}
        , id_{pump_id}
    {
    }
};

struct Version
{
    int major{};
    int minor{};
    int patch{};

    friend constexpr auto operator<=>(const Version&, const Version&) = default;
};

struct DeviceInfo
{
    std::string name;
    Version firmware_version;
};

struct PumpState
{
    AwaitableBool busy_;
};

export class BasicAsyncPodInterface : public std::enable_shared_from_this<BasicAsyncPodInterface>
{
  public:
    virtual ~BasicAsyncPodInterface() = default;

    virtual cobalt::task<void> run() = 0;

    [[nodiscard]] virtual std::unique_ptr<Dispenser> dispenser_for_ingredient(IngredientId ingredient_id) = 0;
    virtual cobalt::promise<void> pump(units::Litre volume, PumpId pump_id) = 0;
    virtual cobalt::promise<void> wait_for_pump_idle(PumpId pump_id) = 0;
};

export template <typename AsyncStream>
class AsyncMachineInterface : public BasicAsyncPodInterface

{
  private:
    log::Logger logger_;
    AsyncMachineProtocolServer<AsyncStream> server_;
    AwaitableBool device_ready_;

    std::unordered_map<IngredientId, PumpId> ingredient_pump_mapping_;
    std::unordered_map<PumpId, std::unique_ptr<PumpState>> pump_states_;

  public:
    AsyncMachineInterface(AsyncStream stream)
        : logger_{log::create_or_get("pod")}
        , server_{std::move(stream)}
        , device_ready_{server_.get_executor()}
    {
    }

    std::unique_ptr<Dispenser> dispenser_for_ingredient(IngredientId ingredient_id) override
    {
        auto it = ingredient_pump_mapping_.find(ingredient_id);
        if (it == ingredient_pump_mapping_.end()) {
            throw DispenserNotFound{ingredient_id};
        }
        return std::make_unique<Pump>(shared_from_this(), it->second);
    }

    cobalt::task<void> run() override
    {
        co_await cobalt::race(monitor_device(), server_.run());
    }

  private:
    cobalt::task<void> monitor_device()
    {
        struct Cleanup
        {
            AsyncMachineInterface& p_;

            ~Cleanup()
            {
                log::debug(p_.logger_, "monitor device loop finished.");
                p_.device_ready_ = false;
            }
        };

        Cleanup c{*this};
        const auto device_info = co_await retry_on_timeout(5, [this](auto timeout) { return aquire_device_info(timeout); });
        log::info(logger_, "Device '{}' is ready.", device_info.name);
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

  public:
    cobalt::promise<DeviceInfo> aquire_device_info(std::chrono::milliseconds timeout = std::chrono::milliseconds{100})
    {
        InDeviceInfoResponse msg = co_await send_and_receive<InDeviceInfoResponse>(OutDeviceInfoRequest{}, timeout);
        co_return DeviceInfo{
            .name = msg.field_deviceName().getValue(),
            .firmware_version =
                Version{
                    .major = msg.field_firmwareMajor().getValue(),
                    .minor = msg.field_firmwareMinor().getValue(),
                    .patch = msg.field_firmwarePatch().getValue(),
                },
        };
    }

    cobalt::promise<void> emergency_stop()
    {
        co_await send_and_receive<InAck>(OutEmergencyStop{});
    }

    cobalt::promise<void> open_valve_for_litre(int valve_id, units::Litre litre);

    cobalt::promise<void> motor_step(int steps);
    cobalt::promise<void> calibrate_pump(int steps_per_litre);

    cobalt::promise<void> pump(units::Litre volume, PumpId pump_id) override
    {
        co_return;
    }

    cobalt::promise<void> wait_for_pump_idle(PumpId pump_id) override
    {
        co_return;
    }

    cobalt::promise<void> load_cell_calibrate_offset(int load_cell_id);
    cobalt::promise<void> load_cell_calibrate_ref_weight(int grams);
    cobalt::promise<int> load_cell_measure_weight();
    cobalt::promise<void> load_cell_set_empty(int grams);

  private:
    PumpState& pump_state(PumpId pump_id)
    {
        auto it = pump_states_.find(pump_id);
        if (it == pump_states_.end()) {
            throw DispenserNotFound{pump_id};
        }
        return *it->second;
    }

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
};

} // namespace cm
