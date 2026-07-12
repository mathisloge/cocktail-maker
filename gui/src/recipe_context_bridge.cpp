module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/cobalt/detached.hpp>
#include <slint.h>
#include <spdlog/spdlog.h>
#include "app-window.h"

module cm.gui:recipe_context_bridge_impl;

import std;
import mp_units;
import cm.core;
import cm;
import :recipe_adapter;
import :machine_adapter;
import :recipe_context_bridge;

namespace cm::gui {

RecipeContextBridge::RecipeContextBridge(boost::asio::any_io_executor executor,
                                         slint::ComponentHandle<AppWindow> ui,
                                         const RecipeStore& recipe_store,
                                         const IngredientStore& ingredient_store,
                                         cm::StationConfig& station_config)
    : executor_{std::move(executor)}
    , ui_{std::move(ui)}
    , recipe_store_{recipe_store}
    , ingredient_store_{ingredient_store}
    , station_config_{station_config}
{
}

void RecipeContextBridge::init()
{
    {
        const auto ingredients = ingredient_store_.ingredients();
        std::vector<slint::SharedString> ingredient_ids;
        ingredient_ids.reserve(ingredients.size());
        std::ranges::transform(ingredients, std::back_inserter(ingredient_ids), [](const auto& ingredient) {
            return slint::SharedString(ingredient.raw().c_str());
        });
        ui_->global<RecipeContext>().set_ingredient_ids(
            std::make_shared<slint::VectorModel<slint::SharedString>>(std::move(ingredient_ids)));
    }
    ui_->global<RecipeContext>().set_recipes(std::make_shared<RecipeModel>(recipe_store_, ingredient_store_));
    ui_->global<RecipeContext>().on_boost_active_recipe(
        [this](const int boost_percentage) { boost_recipe_callback(boost_percentage); });
    ui_->global<cm::gui::RecipeContext>().on_assign_ingredient_to_dispenser([this](const cm::gui::Pod& pod,
                                                                                   const cm::gui::Dispenser& dispenser,
                                                                                   slint::SharedString ingredient_id) {
        SPDLOG_LOGGER_TRACE(
            logger_, "Assign ingredient '{}' to pod '{}' and dispenser '{}'", ingredient_id.data(), pod.id.data(), dispenser.id);
        boost::asio::post(executor_, [&station_config = station_config_, pod, dispenser, ingredient_id]() {
            station_config.update_dispenser_ingredient_mapping(
                cm::IngredientId{ingredient_id.data()},
                {.pod_id = cm::PodId{pod.id.data()}, .dispenser_id = cm::DispenserId{dispenser.id}});
        });
    });
}

void RecipeContextBridge::boost_recipe_callback(const int boost_percentage)
{
    const auto boost = boost_percentage * units::percent;
    const auto active_ui_recipe = ui_->global<RecipeContext>().get_active_recipe();
    auto opt_recipe = recipe_store_.find_by_id(RecipeId{active_ui_recipe.id.data()});
    if (not opt_recipe.has_value()) {
        SPDLOG_LOGGER_ERROR(logger_, "Could not find a recipe with id '{}'", active_ui_recipe.id);
        return;
    }
    auto recipe = opt_recipe.value();
    SPDLOG_LOGGER_TRACE(logger_, "boosting {} by {}.", recipe, boost);
    recipe.commands = cm::boost_recipe(recipe.commands, boost, ingredient_store_);

    ui_->global<RecipeContext>().set_active_recipe(transform(recipe, ingredient_store_));
}
} // namespace cm::gui
