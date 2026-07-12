module;
#include <boost/asio/any_io_executor.hpp>
#include <slint.h>
#include "app-window.h"

export module cm.gui:recipe_context_bridge;

import std;
import cm.core;
import cm;

namespace cm::gui {
export class RecipeContextBridge
{
  public:
    explicit RecipeContextBridge(boost::asio::any_io_executor executor,
                                 slint::ComponentHandle<AppWindow> ui,
                                 const RecipeStore& recipe_store,
                                 const IngredientStore& ingredient_store,
                                 cm::StationConfig& station_config);

    void init();

  private:
    void boost_recipe_callback(const int boost_percentage);

  private:
    log::Logger logger_{log::create_or_get("ui")};
    boost::asio::any_io_executor executor_;
    slint::ComponentHandle<AppWindow> ui_;
    const RecipeStore& recipe_store_;
    const IngredientStore& ingredient_store_;
    cm::StationConfig& station_config_;
};
} // namespace cm::gui
