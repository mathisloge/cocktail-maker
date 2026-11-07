module;
#include <boost/asio/post.hpp>
#include <exec/when_any.hpp>
#include <slint.h>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>
#include "app-window.h"

module cm.gui:process_context_bridge_impl;

import std;
import mp_units;
import cm.core;
import cm;
import :recipe_adapter;
import :machine_adapter;
import :process_context_bridge;
import :mix_error_page;

namespace cm::gui {
slint::SharedString to_slint_string(ErrorItemCategory category)
{
    switch (category) {
    case ErrorItemCategory::Ingredient:
        return "I";
    case ErrorItemCategory::Abort:
        return "A";
    case ErrorItemCategory::Unknown:
        return "U";
    }
    return "U";
}

void ProcessContextBridge::show_mix_error(const std::exception_ptr& error) const
{
    auto page = mix_error_page(error, station_config_, ingredient_store_);
    slint::invoke_from_event_loop([ui = ui_, page = std::move(page)]() {
        auto& process_ctx = ui->global<ProcessContext>();
        process_ctx.set_customer_title(slint::SharedString{page.customer_title});
        process_ctx.set_customer_message(slint::SharedString{page.customer_message});
        process_ctx.set_technical_title(slint::SharedString{page.technical_title});
        process_ctx.set_technical_description(slint::SharedString{page.technical_description});
        process_ctx.set_error_item_category(to_slint_string(page.item_category));
        process_ctx.set_error_item_name(slint::SharedString{page.item_name});
        process_ctx.set_error_item_details(slint::SharedString{page.item_details});
        ui->global<StationStateContext>().invoke_navigate_to(Page::MixErrorPage);
    });
}

ProcessContextBridge::ProcessContextBridge(AsyncScope& async_scope,
                                           slint::ComponentHandle<AppWindow> ui,
                                           const RecipeStore& recipe_store,
                                           const IngredientStore& ingredient_store,
                                           const cm::StationConfig& station_config,
                                           const PodRegistry& pod_registry,
                                           FillLevelTracker& fill_levels)
    : async_scope_{async_scope}
    , ui_{std::move(ui)}
    , recipe_store_{recipe_store}
    , ingredient_store_{ingredient_store}
    , station_config_{station_config}
    , pod_registry_{pod_registry}
    , fill_levels_{fill_levels}
{
}

void ProcessContextBridge::init()
{
    ui_->global<ProcessContext>().on_process_active_recipe([this]() {
        if (ui_->global<ProcessContext>().get_checking_fill_level()) {
            return;
        }
        const auto recipe_to_create = ui_->global<RecipeContext>().get_active_recipe();
        const auto boost = ui_->global<ProcessContext>().get_boost() * units::percent;
        const auto target_volume = ui_->global<ProcessContext>().get_glass_volume() * units::milli_litre;
        auto r = recipe_store_.find_by_id(RecipeId{recipe_to_create.id.data()});
        if (r.has_value()) {
            ui_->global<ProcessContext>().set_checking_fill_level(true);
            async_scope_.spawn(async_check_and_start(PendingMix{std::move(r.value()), boost, target_volume}));
        }
        else {
            show_mix_error(std::make_exception_ptr(RecipeNotFoundError{RecipeId{recipe_to_create.id.data()}}));
            SPDLOG_LOGGER_ERROR(logger_, "Could not find a recipe {}", recipe_to_create.name.data());
        }
    });

    ui_->global<ProcessContext>().on_fill_warning_confirmed([this]() {
        ui_->invoke_close_fill_warning_popup();
        boost::asio::post(async_scope_.executor(), [this]() {
            if (pending_mix_.has_value()) {
                auto mix = std::move(*pending_mix_);
                pending_mix_.reset();
                start_mix(std::move(mix));
            }
        });
    });

    ui_->global<ProcessContext>().on_fill_warning_canceled([this]() {
        ui_->invoke_close_fill_warning_popup();
        boost::asio::post(async_scope_.executor(), [this]() { pending_mix_.reset(); });
    });

    ui_->global<ProcessContext>().on_abort([this]() {
        SPDLOG_LOGGER_INFO(logger_, "Abort recipe processing...");
        show_mix_error(std::make_exception_ptr(AbortError{}));
    });

    ui_->global<StationStateContext>().on_navigated_to([this](Page from, Page to) {
        if (from == Page::MixPage and to != Page::MixSuccessPage and to != Page::MixPage) {
            SPDLOG_LOGGER_INFO(logger_, "Cancelling recipe processing...");
            boost::asio::post(async_scope_.executor(), [this]() { abort_active_recipe(); });
        }
        // A guest who has left the page while the fill levels are checked no longer wants this drink.
        if (to != Page::MixPage) {
            boost::asio::post(async_scope_.executor(), [this]() {
                ++check_generation_;
                pending_mix_.reset();
            });
        }
    });
}

Commands ProcessContextBridge::prepare_commands(const Recipe& recipe,
                                                const units::Percent boost,
                                                const units::Litre target_volume) const
{
    return boost_recipe(scale_recipe(recipe.commands, recipe.nominal_serving_volume, target_volume), boost, ingredient_store_);
}

Task<void> ProcessContextBridge::async_check_and_start(PendingMix mix)
{
    // The confirm button has to come back however this ends.
    struct CheckFinished
    {
        slint::ComponentHandle<AppWindow> ui;

        ~CheckFinished()
        {
            slint::invoke_from_event_loop([ui = ui]() { ui->global<ProcessContext>().set_checking_fill_level(false); });
        }
    } check_finished{ui_};

    pending_mix_.reset();
    const auto generation = ++check_generation_;

    const auto commands = prepare_commands(mix.recipe, mix.boost, mix.target_volume);
    for (const auto& address : involved_dispensers(commands, station_config_)) {
        auto dispenser = pod_registry_.dispenser_of_pod(address.pod_id, address.dispenser_id);
        if (dispenser.has_value()) {
            co_await fill_levels_.refresh(address, **dispenser);
        }
    }

    if (generation != check_generation_) {
        co_return;
    }

    const auto shortages = find_fill_shortages(commands, station_config_, fill_levels_);
    if (shortages.empty()) {
        start_mix(std::move(mix));
        co_return;
    }

    auto lines = std::make_shared<std::vector<std::string>>();
    for (const auto& shortage : shortages) {
        const auto found = ingredient_store_.find_by_id(shortage.ingredient);
        // codespell:ignore-next-line
        lines->push_back(std::format("{}: ca. {} mL da, {} mL nötig",
                                     found.has_value() ? found->display_name : shortage.ingredient.raw(),
                                     std::lround(shortage.available.numerical_value_in(units::milli_litre)),
                                     std::lround(shortage.needed.numerical_value_in(units::milli_litre))));
    }

    pending_mix_ = std::move(mix);
    slint::invoke_from_event_loop([ui = ui_, lines = std::move(lines)]() {
        auto model = std::make_shared<slint::VectorModel<slint::SharedString>>();
        for (const auto& line : *lines) {
            model->push_back(slint::SharedString{line});
        }
        ui->global<ProcessContext>().set_fill_warning_lines(std::move(model));
        ui->invoke_open_fill_warning_popup();
    });
}

void ProcessContextBridge::start_mix(PendingMix mix)
{
    slint::invoke_from_event_loop([ui = ui_]() { ui->global<StationStateContext>().invoke_navigate_to(Page::MixPage); });
    start_recipe_processing(std::move(mix.recipe), mix.boost, mix.target_volume);
}

void ProcessContextBridge::start_recipe_processing(Recipe recipe, const units::Percent boost, const units::Litre target_volume)
{
    abort_active_recipe();

    auto abort = std::make_shared<AwaitableBool>(async_scope_.executor());
    active_recipe_abort_ = abort;

    // An abort stops the processing, which then completes with set_stopped rather than with an exception.
    auto aborted = abort->async_wait() | stdexec::let_value([] { return stdexec::just_stopped(); });
    async_scope_.spawn(exec::when_any(async_process_recipe(std::move(recipe), boost, target_volume), std::move(aborted)) |
                       stdexec::upon_stopped([this, abort]() noexcept {
                           // The capture keeps `abort` alive while the wait on it is pending.
                           SPDLOG_LOGGER_INFO(logger_, "Recipe processing cancelled");
                           async_scope_.spawn(pod_registry_.force_safe_state_all_pods());
                       }));
}

void ProcessContextBridge::abort_active_recipe()
{
    if (active_recipe_abort_ != nullptr) {
        *active_recipe_abort_ = true;
        active_recipe_abort_.reset();
    }
}

Task<void> ProcessContextBridge::async_process_recipe(Recipe recipe, const units::Percent boost, const units::Litre target_volume)
{
    using Clock = std::chrono::steady_clock;

    SPDLOG_LOGGER_INFO(logger_, "Create {} with boost factor '{}' and target volume '{}'", recipe, boost, target_volume);
    const auto start_tp = Clock::now();
    recipe.commands = prepare_commands(recipe, boost, target_volume);
    update_ui_recipe(recipe);
    auto command_executer = std::make_shared<MachineAdapter>(
        ui_, async_scope_.executor(), ingredient_store_, pod_registry_, station_config_, fill_levels_);

    // Failing a sibling only stops the waiting on the Station's side. The Pods keep pouring until they are told otherwise.
    const auto mix_failed = [this](const std::exception_ptr& error) {
        async_scope_.spawn(pod_registry_.force_safe_state_all_pods());
        show_mix_error(error);
    };

    try {
        co_await execute_commands(std::move(recipe.commands), std::move(command_executer));
        const auto duration = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start_tp);
        SPDLOG_LOGGER_INFO(logger_, "Finished {} in '{}'.", recipe, duration);
        display_ui_success(duration);
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_ERROR(logger_, "Recipe processing failed: {}", ex.what());
        mix_failed(std::current_exception());
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
