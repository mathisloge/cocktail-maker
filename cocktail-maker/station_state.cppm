module;
export module cm:station_state;

import std;

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

    virtual void update_state(ConnectionState state) = 0;
};

export class StationState
{
  public:
    virtual ~StationState() = default;

    virtual std::shared_ptr<PodState> create_pod_state() = 0;
};
} // namespace cm
