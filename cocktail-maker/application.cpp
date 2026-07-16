module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/cobalt/spawn.hpp>
#include <boost/cobalt/task.hpp>
#include <mp-units/systems/si/units.h>
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
    const auto recipe_db_path = db_dir / "recipes";
    ingredient_store_.init_ingredients({cm::Ingredient{.id = cm::IngredientId{"test"},
                                                       .display_name = "Test Ingredient",
                                                       .type = cm::IngredientType::other,
                                                       .boost_category = cm::BoostCategory::boostable},
                                        cm::Ingredient{.id = cm::IngredientId{"test2"},
                                                       .display_name = "Test Ingredient2",
                                                       .type = cm::IngredientType::other,
                                                       .boost_category = cm::BoostCategory::reducible}});

    std::vector<cm::Recipe> recipes;
    for (int i = 0; i < 10; i++) {
        recipes.emplace_back(cm::Recipe{
            .id = cm::RecipeId{std::format("mojito_{}", i)},
            .display_name = "Mojito",
            .description = "Der Mojito ist ein erfrischender Cocktail aus Rum, Minze, Limette, Zucker und Soda – perfekt für "
                           "den Sommer.",
            .tags = {std::string{"classic"}},
            .image_path = recipe_db_path / "mojito.png",
            .nominal_serving_volume = 250 * cm::units::milli_litre,
            .commands =
                {
                    cm::DispenseCommand{.ingredient = cm::IngredientId{"test"}, .volume = (75 * cm::units::milli_litre)},
                    cm::ManualCommand{.instruction = "Help me"},
                    cm::ParallelCommand{
                        cm::DispenseCommand{.ingredient = cm::IngredientId{"test"}, .volume = (100 * cm::units::milli_litre)},
                        cm::DispenseCommand{.ingredient = cm::IngredientId{"test2"}, .volume = (50 * cm::units::milli_litre)},
                    },
                },
        });
    }
    recipe_store_.init_recipes(std::move(recipes));
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
