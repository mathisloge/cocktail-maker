module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/cobalt/task.hpp>
#include "app-window.h"

export module cm.gui:station_state_bridge;

import std;
import cm.core;
import cm;
import :operator_auth;

namespace cm::gui {
class PodStateImpl;
class PodUiModel;

export class StationStateBridge : public StationState, public std::enable_shared_from_this<StationStateBridge>
{
  public:
    explicit StationStateBridge(slint::ComponentHandle<AppWindow> ui,
                                const PodRegistry& pod_registry,
                                boost::asio::any_io_executor executor);
    std::unique_ptr<PodState> create_pod_state() override;
    void pod_connection_state_changed(const PodStateImpl& pod);
    void pod_about_to_removed(const PodStateImpl& pod);
    void pod_info_changed(const PodStateImpl& pod);

    std::shared_ptr<slint::Model<gui::Pod>> pod_model() const;

  private:
    std::optional<PodStateImpl*> find_pod(const PodStateImpl* pod);

    void update_pod_model(const PodStateImpl& pod);
    void on_navigate_to(Page target);
    void on_submit_operator_pin(const slint::SharedString& pin, boost::asio::any_io_executor executor);
    boost::cobalt::task<void> async_handle_operator_lockout();

  private:
    log::Logger logger_{log::create_or_get("station_state_bridge")};
    slint::ComponentHandle<AppWindow> ui_;
    std::vector<PodStateImpl*> pods_;
    std::shared_ptr<PodUiModel> pod_model_;
    OperatorAuth operator_auth_;
    bool operator_authenticated_{false};
};

} // namespace cm::gui
