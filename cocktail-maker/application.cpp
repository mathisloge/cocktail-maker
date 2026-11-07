module;
#include <boost/asio/any_io_executor.hpp>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

module cm:application_impl;
import std;
import cm.core;
import :application;
import :pod_discovery;
import :pod_registry;
import :station_state;
import :fill_level;

namespace cm {

namespace {
/// Passes everything on, and keeps the fill levels up to date as the Pod comes and goes.
class FillLevelPodState final : public PodState
{
  public:
    FillLevelPodState(std::unique_ptr<PodState> inner,
                      AsyncScope& scope,
                      const PodRegistry& registry,
                      FillLevelTracker& fill_levels)
        : inner_{std::move(inner)}
        , scope_{scope}
        , registry_{registry}
        , fill_levels_{fill_levels}
    {
    }

    void update_info(PodInfo info) override
    {
        info_ = info;
        inner_->update_info(std::move(info));
    }

    void update_state(ConnectionState state) override
    {
        inner_->update_state(state);
        if (state == ConnectionState::connected) {
            scope_.spawn(sync_pod_fill_levels(info_, registry_, fill_levels_));
        }
        else if (state == ConnectionState::disconnected) {
            fill_levels_.pod_disconnected(info_.id);
        }
    }

  private:
    std::unique_ptr<PodState> inner_;
    PodInfo info_;
    AsyncScope& scope_;
    const PodRegistry& registry_;
    FillLevelTracker& fill_levels_;
};

class FillLevelStationState final : public StationState
{
  public:
    FillLevelStationState(std::shared_ptr<StationState> inner,
                          AsyncScope& scope,
                          const PodRegistry& registry,
                          FillLevelTracker& fill_levels)
        : inner_{std::move(inner)}
        , scope_{scope}
        , registry_{registry}
        , fill_levels_{fill_levels}
    {
    }

    std::unique_ptr<PodState> create_pod_state() override
    {
        auto state = inner_->create_pod_state();
        if (state == nullptr) {
            return nullptr;
        }
        return std::make_unique<FillLevelPodState>(std::move(state), scope_, registry_, fill_levels_);
    }

  private:
    std::shared_ptr<StationState> inner_;
    AsyncScope& scope_;
    const PodRegistry& registry_;
    FillLevelTracker& fill_levels_;
};
} // namespace

Application::Application(const std::filesystem::path& state_dir)
    : glass_store_{state_dir / "glass_active_volumes.json"}
    , station_config_{ingredient_store_, state_dir / "ingredient_dispenser_mapping.json"}
    , fill_levels_{execution_context_.get_executor(), state_dir / "dispenser_fill_levels.json"}
{
}

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
    // The fill levels depend on no catalog. Loaded first, they survive a broken catalog file, which would otherwise leave
    // them empty until the first Pod connects and saves that empty state over the stored one.
    fill_levels_.init();
    glass_store_.init_glasses(load_glasses_from_dir(glass_db_path));
    ingredient_store_.init_ingredients(load_ingredients_from_dir(ingredients_db_path));
    station_config_.init();
    recipe_store_.init_recipes(load_recipes_from_dir(recipe_db_path, ingredient_store_));
}

void Application::run(std::shared_ptr<StationState> station_state, std::unique_ptr<PodDiscovery> pod_discovery)
{
    execution_thread_ = std::make_unique<std::jthread>([this]() {
        SPDLOG_LOGGER_INFO(logger_, "Async context starting.");
        execution_context_.run();
        SPDLOG_LOGGER_INFO(logger_, "Async context finished.");
    });
    auto tracked_state =
        std::make_shared<FillLevelStationState>(std::move(station_state), async_scope_, pod_registry_, fill_levels_);
    async_scope_.spawn(discover_and_run_pods(std::move(pod_discovery), std::move(tracked_state), pod_registry_, async_scope_));
}

void Application::shutdown()
{
    if (execution_thread_ == nullptr) {
        return;
    }

    SPDLOG_LOGGER_INFO(logger_, "Stopping all asynchronous work...");

    // The scope is stopped and joined on the execution thread. The calling thread only waits for it.
    stdexec::sync_wait(stdexec::starts_on(async_scope_.scheduler(), async_scope_.stop_and_join()));

    // All asynchronous work has finished, so the handlers that are still queued can be dropped.
    work_guard_.reset();
    execution_context_.stop();
    execution_thread_.reset();
    fill_levels_.flush();
    station_config_.flush();
    glass_store_.flush();
}

boost::asio::any_io_executor Application::get_executor()
{
    return execution_context_.get_executor();
}

AsyncScope& Application::async_scope()
{
    return async_scope_;
}

FillLevelTracker& Application::fill_levels()
{
    return fill_levels_;
}
} // namespace cm
