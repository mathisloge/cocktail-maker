module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/cobalt/task.hpp>
#include "app-window.h"

export module cm.gui:process_context_bridge;

import std;
import cm.core;
import cm;

namespace cm::gui {
export class ProcessContextBridge
{
  public:
    explicit ProcessContextBridge(boost::asio::any_io_executor executor,
                                  slint::ComponentHandle<AppWindow> ui,
                                  const RecipeStore& recipe_store,
                                  const IngredientStore& ingredient_store,
                                  const cm::StationConfig& station_config,
                                  const PodRegistry& pod_registry);

    void init();

  private:
    boost::cobalt::task<void> async_process_recipe(Recipe recipe, units::Percent boost, units::Litre target_volume);

    void update_ui_recipe(const Recipe& recipe) const;

    void display_ui_success(std::chrono::milliseconds duration) const;

  private:
    log::Logger logger_{log::create_or_get("ui")};
    boost::asio::any_io_executor executor_;
    slint::ComponentHandle<AppWindow> ui_;
    const PodRegistry& pod_registry_;
    const RecipeStore& recipe_store_;
    const IngredientStore& ingredient_store_;
    const cm::StationConfig& station_config_;
    boost::asio::cancellation_signal active_cancel_signal_;
};
} // namespace cm::gui
