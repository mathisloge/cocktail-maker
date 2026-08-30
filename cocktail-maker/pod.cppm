module;
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/task.hpp>
#include <proto/field/ErrorCodeCommon.h>

export module cm:pod;

import std;
import mp_units;
import cm.core;
import :pod_types;
import :station_state;
import :pod_protocol_session;
import :dispenser;

namespace cobalt = boost::cobalt;

using namespace std::chrono_literals;

namespace cm {
export class PodReceiveError : public std::runtime_error
{
  public:
    PodReceiveError(PodId pod_id, proto::field::ErrorCodeVal error)
        : runtime_error{
              std::format("Pod '{}' NAK with error reason: {}", pod_id, proto::field::ErrorCodeCommon::valueName(error))}
        , pod_id_{pod_id}
        , error_code_{error}
    {
    }

    PodId pod_id() const
    {
        return pod_id_;
    }

    proto::field::ErrorCodeVal error_code() const
    {
        return error_code_;
    }

  private:
    PodId pod_id_;
    proto::field::ErrorCodeVal error_code_;
};

export class PodTimeoutError : public TimeoutError
{
  public:
    explicit PodTimeoutError(PodId pod_id)
        : TimeoutError{std::format("Pod '{}' did not respond within the timeout", pod_id)}
        , pod_id_{pod_id}
    {
    }

    PodId pod_id() const
    {
        return pod_id_;
    }

  private:
    PodId pod_id_;
};

export class DispenserEmptyError : public std::runtime_error
{
  public:
    DispenserEmptyError(PodId pod_id, DispenserId dispenser_id);

    PodId pod_id() const
    {
        return pod_id_;
    }

    DispenserId dispenser_id() const
    {
        return dispenser_id_;
    }

  private:
    PodId pod_id_;
    DispenserId dispenser_id_;
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

class NoopPodState final : public PodState
{
  public:
    void update_info(PodInfo info) override {};
    void update_state([[maybe_unused]] ConnectionState state) override {};
};

class DispenserPodImpl : public Dispenser
{
  public:
    explicit DispenserPodImpl(std::weak_ptr<IPod> pod, DispenserId dispenser_id, std::string logger_name);

    cobalt::promise<units::Litre> dispense(units::Litre volume) override;

    cobalt::promise<void> load_cell_calibrate_with_ref_weight(units::Grams grams) override;

    cobalt::promise<void> load_cell_tare() override;

    cobalt::promise<void> highlight(std::chrono::milliseconds duration) override;

  protected:
    std::shared_ptr<IPod> pod();

    DispenserId id() const;

  protected:
    log::Logger logger_;

  private:
    const DispenserId dispenser_id_;
    std::weak_ptr<IPod> pod_;
};

export class Pump final : public DispenserPodImpl
{

  public:
    Pump(std::weak_ptr<IPod> pod, DispenserId dispenser_id);
    cobalt::promise<units::Litre> calibrate(units::Steps steps);
};

export class Valve final : public DispenserPodImpl
{
  public:
    Valve(std::weak_ptr<IPod> pod, DispenserId dispenser_id);
};

export class Pod : public IPod, public std::enable_shared_from_this<Pod>
{
  public:
    explicit Pod(std::unique_ptr<AnyIoStream> stream);

    PodId pod_id() const override;

    cobalt::task<void> run(std::unique_ptr<PodState> state) override;

    std::expected<std::unique_ptr<Dispenser>, DispenserNotFoundError> create_dispenser(DispenserId dispenser_id) override;

    cobalt::promise<PodInfo> aquire_device_info(std::chrono::milliseconds timeout = 100ms);

    cobalt::promise<void> load_cell_calibrate_with_ref_weight(const DispenserId dispenser_id, const units::Grams grams) override;

    cobalt::promise<void> load_cell_tare(DispenserId dispenser_id) override;

    cobalt::promise<units::Litre> dispense(DispenserId dispenser_id, units::Litre volume) override;

    cobalt::promise<units::Litre> pump_calibrate(const DispenserId dispenser_id, const units::Steps steps) override;

    cobalt::promise<void> highlight_dispenser(DispenserId dispenser_id, std::chrono::milliseconds duration) override;

    cobalt::task<void> force_safe_state() override;

  private:
    cobalt::task<void> monitor_device();

    cobalt::task<void> keep_alive();

  private:
    log::Logger logger_{log::create_or_get("pod")};
    std::unique_ptr<PodState> state_{std::make_unique<NoopPodState>()};
    PodProtocolSession session_;
    AwaitableBool device_ready_;
};
} // namespace cm
