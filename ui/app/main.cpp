#include <QApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <cm/execution_context.hpp>
#include <cm/liquid_dispenser_simulated.hpp>
#include <cm/recipe.hpp>
#include <cm/recipe_store.hpp>
#include "ApplicationState.hpp"
#include "cm/commands/dispense_liquid_cmd.hpp"
#include "cm/commands/manual_cmd.hpp"
#include <mp-units/systems/imperial.h>
#include <mp-units/systems/international.h>

Q_IMPORT_QML_PLUGIN(CocktailMaker_UiPlugin)
using namespace cm;
int main(int argc, char *argv[])
{
    QApplication app{argc, argv};
    QCoreApplication::setApplicationName(QStringLiteral("CocktailMaker"));
    QCoreApplication::setOrganizationName(QStringLiteral("com.mathisloge.cocktail-maker"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QT_VERSION_STR));

    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    app.setPalette(palette);

    QQmlApplicationEngine engine;

    auto &&app_state = engine.singletonInstance<cm::app::ApplicationState *>("CocktailMaker.App", "ApplicationState");
    Q_ASSERT(app_state != nullptr);

    boost::asio::thread_pool thread_pool{3};
    std::shared_ptr<cm::ExecutionContext> execution_context =
        std::make_shared<cm::ExecutionContext>(thread_pool.get_executor());

    auto ingredient_store = std::make_shared<cm::IngredientStore>();
    auto recipe_store = std::make_shared<cm::RecipeStore>();
    auto recipe_factory = std::make_shared<cm::ui::RecipeFactory>(recipe_store, ingredient_store);
    auto recipe_executor = std::make_shared<cm::ui::RecipeExecutorAdapter>(execution_context, ingredient_store);

    auto &&water = ingredient_store->add_ingredient(Ingredient{.id = "water", .display_name = "Wasser"});
    auto &&bacardi = ingredient_store->add_ingredient(Ingredient{.id = "bacardi", .display_name = "Bacardi"});
    auto &&soda = ingredient_store->add_ingredient(Ingredient{.id = "soda", .display_name = "Soda Wasser"});
    auto &&lime_juice =
        ingredient_store->add_ingredient(Ingredient{.id = "lime_juice", .display_name = "Limettensaft"});

    execution_context->liquid_registry().register_dispenser(
        water.id,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre,
                                                       (0.1 * units::si::litre) / (0.5 * units::si::second)));
    execution_context->liquid_registry().register_dispenser(
        bacardi.id,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre,
                                                       (0.1 * units::si::litre) / (0.5 * units::si::second)));
    execution_context->liquid_registry().register_dispenser(
        soda.id,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre,
                                                       (0.1 * units::si::litre) / (2 * units::si::second)));
    execution_context->liquid_registry().register_dispenser(
        lime_juice.id,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre,
                                                       (0.1 * units::si::litre) / (1 * units::si::second)));

    auto only_water = cm::make_recipe()
                          .with_name("Only Water")
                          .with_description("Klassisches Wasser ohne Schickschnack")
                          .with_steps()
                          .with_step(std::make_unique<cm::DispenseLiquidCmd>(
                              water.id, 250 * units::si::milli<units::si::litre>, generate_unique_command_id()))
                          .add()
                          .with_steps()
                          .with_step(std::make_unique<cm::ManualCmd>("2 Eiswürfel", generate_unique_command_id()))
                          .add()
                          .create();

    auto mojito =
        cm::make_recipe()
            .with_name("Mojito")
            .with_description(
                "Der Mojito ist ein erfrischender Cocktail aus Rum, Minze, Limette, Zucker und " // codespell:ignore
                "Soda – "                                                                        // codespell:ignore
                "perfekt für den Sommer.")                                                       // codespell:ignore
            .with_steps()
            .with_step(std::make_unique<cm::DispenseLiquidCmd>(
                bacardi.id, 3 * units::imperial::fluid_ounce, generate_unique_command_id()))
            .with_step(std::make_unique<cm::DispenseLiquidCmd>(
                soda.id, 120 * units::si::milli<units::si::litre>, generate_unique_command_id()))
            .with_step(std::make_unique<cm::DispenseLiquidCmd>(
                lime_juice.id, 30 * mp_units::si::milli<mp_units::si::litre>, generate_unique_command_id()))
            .add()
            .with_steps()
            .with_step(
                std::make_unique<cm::ManualCmd>("2 Minzblätter", generate_unique_command_id())) // codespell:ignore
            .add()
            .with_steps()
            .with_step(std::make_unique<cm::ManualCmd>("2 TL Zucker", generate_unique_command_id())) // codespell:ignore
            .add()
            .create();
    recipe_store->add_recipe(only_water);
    recipe_store->add_recipe(std::move(mojito));

    app_state->recipe_store = recipe_store;
    app_state->recipe_factory = recipe_factory;
    app_state->recipe_executor = recipe_executor;

    engine.loadFromModule("CocktailMaker.App", "Main");

    return app.exec();
}
