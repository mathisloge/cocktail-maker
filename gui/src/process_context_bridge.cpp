module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/cobalt/detached.hpp>
#include <boost/cobalt/spawn.hpp>
#include <slint.h>
#include <spdlog/spdlog.h>
#include "app-window.h"

module cm.gui:process_context_bridge_impl;

import std;
import mp_units;
import cm.core;
import cm;
import :recipe_adapter;
import :machine_adapter;
import :process_context_bridge;

namespace cm::gui {
enum class ErrorItemCategory
{
    Ingredient,
    Unknown
};

slint::SharedString to_slint_string(ErrorItemCategory category)
{
    switch (category) {
    case ErrorItemCategory::Ingredient:
        return "I";
    case ErrorItemCategory::Unknown:
        return "U";
    }
    return "U";
}

struct ErrorPageData
{
    slint::SharedString title;
    slint::SharedString description;
    ErrorItemCategory item_category = ErrorItemCategory::Unknown;
    slint::SharedString item_name;
    slint::SharedString item_details;
};

namespace error_page {
ErrorPageData make(const DispenserEmptyError& ex,
                   const cm::StationConfig& station_config,
                   const IngredientStore& ingredient_store)
{
    const auto dispenser = PodDispenser{ex.pod_id(), ex.dispenser_id()};

    const std::expected<Ingredient, std::out_of_range> ingredient =
        station_config.find_ingredient_by_dispenser(dispenser).and_then(
            [&](IngredientId id) -> std::expected<Ingredient, std::out_of_range> {
                if (auto opt = ingredient_store.find_by_id(id); opt.has_value()) {
                    return std::move(opt).value();
                }
                return std::unexpected(std::out_of_range{"Konnte Ingredient für Behälter nicht finden"});
            });

    const auto item_details = std::format("Bitte Behälter '{}' von Pod '{}' befüllen", ex.dispenser_id(), ex.pod_id());

    if (!ingredient.has_value()) {
        // Dispenser->ingredient mapping missing or ingredient not in store -
        // still report the dispenser-empty error, just without a friendly name.
        return ErrorPageData{
            .title = "Behälter leer!",
            .description = "Bitte auffüllen.",
            .item_category = ErrorItemCategory::Unknown,
            .item_name = "Unbekannte Zutat",
            .item_details = item_details.c_str(),
        };
    }

    return ErrorPageData{
        .title = "Behälter leer!",
        .description = "Bitte auffüllen.",
        .item_category = ErrorItemCategory::Ingredient,
        .item_name = ingredient->display_name.c_str(),
        .item_details = item_details.c_str(),
    };
}

// Fallback for any exception without a dedicated overload above.
ErrorPageData make(const std::exception& ex, const cm::StationConfig&, const IngredientStore&)
{
    return ErrorPageData{
        .title = "Unbekannter Fehler!",
        .description = ex.what(),
        .item_category = ErrorItemCategory::Unknown,
        .item_name = "Unbekannt",
        .item_details = "Fehler",
    };
}

} // namespace error_page

void display_ui_error(const std::derived_from<std::exception> auto& ex,
                      slint::ComponentHandle<AppWindow> ui,
                      const cm::StationConfig& station_config,
                      const IngredientStore& ingredient_store)
{
    auto data = error_page::make(ex, station_config, ingredient_store);

    slint::invoke_from_event_loop([ui, data = std::move(data)]() mutable {
        auto& process_ctx = ui->global<ProcessContext>();
        process_ctx.set_error_title(std::move(data.title));
        process_ctx.set_error_description(std::move(data.description));
        process_ctx.set_error_item_category(to_slint_string(data.item_category));
        process_ctx.set_error_item_name(std::move(data.item_name));
        process_ctx.set_error_item_details(std::move(data.item_details));
        ui->global<StationStateContext>().invoke_navigate_to(Page::MixErrorPage);
    });
}

ProcessContextBridge::ProcessContextBridge(boost::asio::any_io_executor executor,
                                           slint::ComponentHandle<AppWindow> ui,
                                           const RecipeStore& recipe_store,
                                           const IngredientStore& ingredient_store,
                                           const cm::StationConfig& station_config,
                                           const PodRegistry& pod_registry)
    : executor_{std::move(executor)}
    , ui_{std::move(ui)}
    , recipe_store_{recipe_store}
    , ingredient_store_{ingredient_store}
    , station_config_{station_config}
    , pod_registry_{pod_registry}
{
}

void ProcessContextBridge::init()
{
    ui_->global<ProcessContext>().on_process_active_recipe([this]() {
        const auto recipe_to_create = ui_->global<RecipeContext>().get_active_recipe();
        const auto boost = ui_->global<ProcessContext>().get_boost() * units::percent;
        const auto target_volume = ui_->global<ProcessContext>().get_glass_volume() * units::milli_litre;
        auto r = recipe_store_.find_by_id(RecipeId{recipe_to_create.id.data()});
        if (r.has_value()) {
            ui_->global<StationStateContext>().invoke_navigate_to(Page::MixPage);
            boost::asio::post(executor_, [recipe = std::move(r.value()), boost, target_volume, this]() {
                active_cancel_signal_.emit(boost::asio::cancellation_type::all);
                boost::cobalt::spawn(executor_,
                                     async_process_recipe(recipe, boost, target_volume),
                                     boost::asio::bind_cancellation_slot(active_cancel_signal_.slot(), boost::asio::detached));
            });
        }
        else {
            // TODO: Add RecipeNotFoundError
            display_ui_error(std::runtime_error{"Could not find recipe"}, ui_, station_config_, ingredient_store_);
            SPDLOG_LOGGER_ERROR(logger_, "Could not find a recipe {}", recipe_to_create.name.data());
        }
    });

    ui_->global<StationStateContext>().on_navigated_to([this](Page from, Page to) {
        if (from == Page::MixPage and to != Page::MixSuccessPage) {
            SPDLOG_LOGGER_INFO(logger_, "Cancelling recipe processing...");
            boost::asio::post(executor_, [this]() { active_cancel_signal_.emit(boost::asio::cancellation_type::all); });
        }
    });
}

cobalt::task<void> ProcessContextBridge::async_process_recipe(Recipe recipe,
                                                              const units::Percent boost,
                                                              const units::Litre target_volume)
{
    using Clock = std::chrono::steady_clock;

    SPDLOG_LOGGER_DEBUG(logger_, "Create {} with boost factor '{}' and target volume '{}'", recipe, boost, target_volume);
    const auto start_tp = Clock::now();
    recipe.commands = scale_recipe(recipe.commands, recipe.nominal_serving_volume, target_volume);
    recipe.commands = boost_recipe(recipe.commands, boost, ingredient_store_);
    update_ui_recipe(recipe);
    auto command_executer = std::make_shared<MachineAdapter>(ui_, ingredient_store_, pod_registry_, station_config_);
    try {
        co_await execute_commands(std::move(recipe.commands), std::move(command_executer));
        const auto duration = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start_tp);
        SPDLOG_LOGGER_INFO(logger_, "Finished {} in '{}'"., recipe, duration);
        display_ui_success(duration);
    }
    catch (const boost::system::system_error& ex) {
        if (ex.code() == boost::asio::error::operation_aborted) {
            SPDLOG_LOGGER_INFO(logger_, "Recipe processing cancelled");
            cobalt::spawn(executor_, pod_registry_.force_safe_state_all_pods(), boost::asio::detached);
            co_return; // clean exit, no UI error
        }
        SPDLOG_LOGGER_ERROR(logger_, "System error while processing recipe: {}", ex.what());
        display_ui_error(ex, ui_, station_config_, ingredient_store_);
    }
    catch (const DispenserEmptyError& ex) {
        display_ui_error(ex, ui_, station_config_, ingredient_store_);
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_ERROR(logger_, "Unknown error while processing recipe: {}", ex.what());
        display_ui_error(ex, ui_, station_config_, ingredient_store_);
    }
}

void ProcessContextBridge::update_ui_recipe(const Recipe& recipe) const
{
    slint::invoke_from_event_loop([ui = ui_, recipe = std::move(recipe), &ingredient_store = ingredient_store_]() {
        auto ui_recipe = ui->global<RecipeContext>().get_active_recipe();
        ui_recipe.commands = transform(recipe.commands, ingredient_store);
        ui->global<RecipeContext>().set_active_recipe(ui_recipe);
    });
}

void ProcessContextBridge::display_ui_success(const std::chrono::milliseconds duration) const
{
    slint::invoke_from_event_loop([ui = ui_, duration]() {
        const auto& process_ctx = ui->global<ProcessContext>();
        process_ctx.set_elapsed_time(duration.count());
        ui->global<StationStateContext>().invoke_navigate_to(Page::MixSuccessPage);
    });
}

} // namespace cm::gui
