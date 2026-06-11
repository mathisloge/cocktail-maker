module;
export module cm:station_state;

import std;
import :pod_id;

namespace cm {
export class PodState
{
  public:
    enum class ConnectionState
    {
        unknown,
        connecting,
        connected,
        disconnected,
    };

  public:
    virtual ~PodState() = default;

    virtual void update_id(PodId pod_id) = 0;
    virtual void update_state(ConnectionState state) = 0;
};

export class StationState
{
  public:
    virtual ~StationState() = default;

    virtual std::unique_ptr<PodState> create_pod_state() = 0;
};
} // namespace cm
