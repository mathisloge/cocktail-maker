module;
#include "app-window.h"

export module cm.gui:station_state_bridge;

import std;
import cm.core;
import cm;
import :operator_auth;

namespace cm::gui {
class PodStateImpl;
class PodUiModel;

/// What the admin area says and shows about a bottle.
export struct FillPresentation
{
    std::string text;
    /// Whether the bottle's content is known, so that a fill bar makes sense.
    bool known{};
    /// The remaining share of the bottle's content, from 0 to 1.
    float ratio{};
    bool almost_empty{};
};

export [[nodiscard]] FillPresentation present_fill_level(const FillLevelView& view);

/// The name the admin area gives a dispenser, numbered per type from 1.
export [[nodiscard]] std::string dispenser_label(DispenserType type, int number);

export class StationStateBridge : public StationState, public std::enable_shared_from_this<StationStateBridge>
{
  public:
    explicit StationStateBridge(slint::ComponentHandle<AppWindow> ui,
                                const PodRegistry& pod_registry,
                                AsyncScope& async_scope,
                                FillLevelTracker& fill_levels);
    std::unique_ptr<PodState> create_pod_state() override;

    /// Shows the name of the assigned ingredient on each dispenser, now and after every change of the assignment.
    void watch_assignments(StationConfig& station_config, const IngredientStore& ingredient_store);
    void pod_connection_state_changed(const PodStateImpl& pod);
    void pod_about_to_removed(const PodStateImpl& pod);
    void pod_info_changed(const PodStateImpl& pod);

    std::shared_ptr<slint::Model<gui::Pod>> pod_model() const;

  private:
    std::optional<PodStateImpl*> find_pod(const PodStateImpl* pod);

    void update_pod_model(const PodStateImpl& pod);
    void on_navigate_to(Page target);
    void on_submit_operator_pin(const slint::SharedString& pin);
    void publish_operator_state(std::chrono::steady_clock::time_point now);

  private:
    log::Logger logger_{log::create_or_get("station_state_bridge")};
    slint::ComponentHandle<AppWindow> ui_;
    std::vector<PodStateImpl*> pods_;
    std::shared_ptr<PodUiModel> pod_model_;
    OperatorAuth operator_auth_;
    bool operator_authenticated_{false};
    // The countdown runs on the UI thread, which is the only one that touches operator_auth_.
    slint::Timer lockout_timer_;
};

} // namespace cm::gui
