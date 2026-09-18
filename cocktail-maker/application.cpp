module;
#include <boost/asio/any_io_executor.hpp>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

module cm:application_impl;
import std;
import cm.core;
import :application;
import :pod_discovery;

namespace cm {

Application::~Application()
{
    try {
        shutdown();
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_ERROR(logger_, "Could not shut down cleanly: {}", ex.what());
    }
}

void Application::init(const std::filesystem::path& db_dir)
{
    const auto glass_db_path = db_dir / "glasses";
    const auto ingredients_db_path = db_dir / "ingredients";
    const auto recipe_db_path = db_dir / "recipes";
    glass_store_.init_glasses(load_glasses_from_dir(glass_db_path));
    ingredient_store_.init_ingredients(load_ingredients_from_dir(ingredients_db_path));
    recipe_store_.init_recipes(load_recipes_from_dir(recipe_db_path, ingredient_store_));
    station_config_.init();
}

void Application::run(std::shared_ptr<StationState> station_state, std::unique_ptr<PodDiscovery> pod_discovery)
{
    execution_thread_ = std::make_unique<std::jthread>([this]() {
        SPDLOG_LOGGER_INFO(logger_, "Async context starting.");
        execution_context_.run();
        SPDLOG_LOGGER_INFO(logger_, "Async context finished.");
    });
    async_scope_.spawn(discover_and_run_pods(std::move(pod_discovery), std::move(station_state), pod_registry_, async_scope_));
}

void Application::shutdown()
{
    if (execution_thread_ == nullptr) {
        return;
    }

    SPDLOG_LOGGER_INFO(logger_, "Stopping all asynchronous work...");

    // The scope is stopped and joined on the execution thread. The calling thread only waits for it.
    auto stop_and_join = [](AsyncScope& scope) -> Task<void> {
        scope.request_stop();
        co_await scope.join();
    };
    stdexec::sync_wait(stdexec::starts_on(async_scope_.scheduler(), stop_and_join(async_scope_)));

    // All asynchronous work has finished, so the handlers that are still queued can be dropped.
    work_guard_.reset();
    execution_context_.stop();
    execution_thread_.reset();
}

boost::asio::any_io_executor Application::get_executor()
{
    return execution_context_.get_executor();
}

AsyncScope& Application::async_scope()
{
    return async_scope_;
}
} // namespace cm
