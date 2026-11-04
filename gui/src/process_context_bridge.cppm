module;
#include "app-window.h"

export module cm.gui:process_context_bridge;

import std;
import mp_units;
import cm.core;
import cm;

namespace cm::gui {
export class ProcessContextBridge
{
  public:
    explicit ProcessContextBridge(AsyncScope& async_scope,
                                  slint::ComponentHandle<AppWindow> ui,
                                  const RecipeStore& recipe_store,
                                  const IngredientStore& ingredient_store,
                                  const cm::StationConfig& station_config,
                                  const PodRegistry& pod_registry,
                                  FillLevelTracker& fill_levels);

    void init();

  private:
    /// A mix the guest has asked for, waiting for the check of the fill levels or for the answer to its warning.
    struct PendingMix
    {
        Recipe recipe;
        units::Percent boost;
        units::Litre target_volume;
    };

    /// Reads the bottles a mix needs, then starts it or warns the guest.
    Task<void> async_check_and_start(PendingMix mix);

    /// Leaves the current page and starts processing. It runs on the io thread.
    void start_mix(PendingMix mix);

    Commands prepare_commands(const Recipe& recipe, units::Percent boost, units::Litre target_volume) const;

    /// Aborts a recipe that may still be processed and starts processing `recipe`. It runs on the io thread.
    void start_recipe_processing(Recipe recipe, units::Percent boost, units::Litre target_volume);

    /// Stops the recipe that is currently processed, if there is one. It runs on the io thread.
    void abort_active_recipe();

    Task<void> async_process_recipe(Recipe recipe, units::Percent boost, units::Litre target_volume);

    void update_ui_recipe(const Recipe& recipe) const;

    void display_ui_success(std::chrono::milliseconds duration) const;

  private:
    log::Logger logger_{log::create_or_get("ui")};
    AsyncScope& async_scope_;
    slint::ComponentHandle<AppWindow> ui_;
    const PodRegistry& pod_registry_;
    const RecipeStore& recipe_store_;
    const IngredientStore& ingredient_store_;
    const cm::StationConfig& station_config_;
    FillLevelTracker& fill_levels_;
    /// Setting it aborts the recipe that is currently processed. It is only accessed on the io thread.
    std::shared_ptr<AwaitableBool> active_recipe_abort_;
    /// The mix whose warning is open. It is only accessed on the io thread.
    std::optional<PendingMix> pending_mix_;
};
} // namespace cm::gui
