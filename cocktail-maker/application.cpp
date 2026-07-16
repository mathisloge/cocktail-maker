module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/cobalt/spawn.hpp>
#include <boost/cobalt/task.hpp>
#include <spdlog/spdlog.h>

module cm:application_impl;
import std;
import :application;
import :pod_discovery;

namespace cm {

Application::~Application()
{
    execution_context_.stop();
}

void Application::init(const std::filesystem::path& db_dir)
{
    const auto glass_db_path = db_dir / "glasses";
    const auto ingredients_db_path = db_dir / "ingredients";
    const auto recipe_db_path = db_dir / "recipes";
    glass_store_.init_glasses(load_glasses_from_dir(glass_db_path));
    ingredient_store_.init_ingredients(load_ingredients_from_dir(ingredients_db_path));
    recipe_store_.init_recipes(load_recipes_from_dir(recipe_db_path, ingredient_store_));
}

void Application::run(std::shared_ptr<StationState> station_state, std::unique_ptr<PodDiscovery> pod_discovery)
{
    execution_thread_ = std::make_unique<std::jthread>(
        [this, station_state = std::move(station_state), pod_discovery = std::move(pod_discovery)]() mutable {
            auto logger = cm::log::create_or_get("app");
            boost::cobalt::this_thread::set_executor(execution_context_.get_executor());
            SPDLOG_LOGGER_INFO(logger, "Async context starting.");
            boost::cobalt::spawn(execution_context_.get_executor(),
                                 discover_and_run_pods(std::move(pod_discovery), std::move(station_state), pod_registry_),
                                 boost::asio::detached);
            execution_context_.run();
            SPDLOG_LOGGER_INFO(logger, "Async context finished.");
        });
}

boost::asio::any_io_executor Application::get_executor()
{
    return execution_context_.get_executor();
}
} // namespace cm
