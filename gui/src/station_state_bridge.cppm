module;
#include <boost/asio/any_io_executor.hpp>
#include "app-window.h"

export module cm.gui:station_state_bridge;

import std;
import cm;

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

  private:
    slint::ComponentHandle<AppWindow> ui_;
    std::vector<PodStateImpl*> pods_;
    std::shared_ptr<PodUiModel> pod_model_;
};

} // namespace cm::gui
