module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
export module cm:application;

import std;
import cm.core;
import :pod_discovery;
import :pod_registry;
import :ingredient;
import :recipe;
import :glass;
import :station_config;
import :station_state;

namespace cm {
export class Application
{
  public:
    virtual ~Application();

    virtual void init(const std::filesystem::path& db_dir);
    boost::asio::any_io_executor get_executor();
    AsyncScope& async_scope();

    /**
     * Stops all asynchronous work, waits until it has finished and ends the execution thread.
     *
     * It must be called before anything that the asynchronous work references is destroyed. Calling it again, or without a
     * running application, has no effect.
     */
    void shutdown();

  protected:
    void run(std::shared_ptr<StationState> station_state, std::unique_ptr<PodDiscovery> pod_discovery);

  protected:
    GlassStore glass_store_;
    IngredientStore ingredient_store_;
    RecipeStore recipe_store_;
    PodRegistry pod_registry_;
    StationConfig station_config_{ingredient_store_, "./station_config.json"};

  private:
    log::Logger logger_{log::create_or_get("app")};
    boost::asio::io_context execution_context_;
    boost::asio::executor_work_guard<decltype(execution_context_.get_executor())> work_guard_{
        boost::asio::make_work_guard(execution_context_.get_executor())};
    AsyncScope async_scope_{execution_context_};
    std::unique_ptr<std::jthread> execution_thread_;
};

} // namespace cm
