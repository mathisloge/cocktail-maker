module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/cobalt/thread.hpp>
export module cm:application;

import std;
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

  protected:
    void run(std::shared_ptr<StationState> station_state, std::unique_ptr<PodDiscovery> pod_discovery);

  protected:
    GlassStore glass_store_;
    IngredientStore ingredient_store_;
    RecipeStore recipe_store_;
    PodRegistry pod_registry_;
    StationConfig station_config_{ingredient_store_, "./station_config.json"};

  private:
    boost::asio::io_context execution_context_;
    std::unique_ptr<std::jthread> execution_thread_;
    boost::asio::executor_work_guard<decltype(execution_context_.get_executor())> work_guard_{
        boost::asio::make_work_guard(execution_context_.get_executor())};
};

} // namespace cm
