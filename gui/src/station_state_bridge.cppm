module;
#include <slint.h>
#include "app-window.h"

export module cm.gui:station_state_bridge;

import std;
import cm;

namespace cm::gui {

export class StationStateBridge;

class PodStateImpl : public PodState
{
  public:
    PodStateImpl(std::shared_ptr<StationStateBridge> parent)
        : parent_{std::move(parent)}
    {
    }

    ConnectionState connection_state() const
    {
        return connection_state_;
    }

    void update_state(ConnectionState state) override;

  private:
    std::weak_ptr<StationStateBridge> parent_;
    ConnectionState connection_state_{};
};

class StationStateBridge : public StationState, public std::enable_shared_from_this<StationStateBridge>
{
  public:
    explicit StationStateBridge(slint::ComponentHandle<AppWindow> ui)
        : ui_{std::move(ui)}
    {
    }

    std::shared_ptr<PodState> create_pod_state() override
    {
        auto pod_state = std::make_shared<PodStateImpl>(shared_from_this());
        pods_.emplace_back(pod_state);
        return pod_state;
    }

    void pod_connection_state_changed()
    {
        const bool all_connected = (pods_.size() > 0) and std::ranges::all_of(pods_, [](const auto& p) {
                                       return p->connection_state() == PodState::ConnectionState::connected;
                                   });

        // update device ready to all_connected
        slint::invoke_from_event_loop([connected = all_connected, ui = ui_]() { ui->set_device_ready(connected); });
    }

  private:
    slint::ComponentHandle<AppWindow> ui_;
    std::vector<std::shared_ptr<PodStateImpl>> pods_;
};

void PodStateImpl::update_state(ConnectionState state)
{
    connection_state_ = state;
    if (auto p = parent_.lock(); p != nullptr) {
        p->pod_connection_state_changed();
    }
}
} // namespace cm::gui