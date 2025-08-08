// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <cm/commands/dispense_liquid_cmd.hpp>
#include <cm/commands/manual_cmd.hpp>
#include <cm/execution_context.hpp>
#include <cm/glass_store.hpp>
#include <cm/liquid_dispenser_simulated.hpp>
#include <cm/recipe.hpp>
#include <cm/recipe_store.hpp>
#include <mp-units/systems/imperial.h>
#include <mp-units/systems/international.h>

#include "ApplicationState.hpp"

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

    boost::asio::thread_pool thread_pool{3};
    std::shared_ptr<cm::ExecutionContext> execution_context =
        std::make_shared<cm::ExecutionContext>(thread_pool.get_executor());

    auto ingredient_store = std::make_shared<cm::IngredientStore>();
    auto recipe_store = std::make_shared<cm::RecipeStore>();
    auto recipe_factory = std::make_shared<cm::ui::RecipeFactory>(recipe_store, ingredient_store);
    auto recipe_executor = std::make_shared<cm::ui::RecipeExecutorAdapter>(execution_context, ingredient_store);

    auto &&water = ingredient_store->add_ingredient(
        Ingredient{.id = "water", .display_name = "Wasser", .boost_category = BoostCategory::reducible});
    auto &&bacardi = ingredient_store->add_ingredient(
        Ingredient{.id = "bacardi", .display_name = "Bacardi", .boost_category = BoostCategory::boostable});
    auto &&soda = ingredient_store->add_ingredient(
        Ingredient{.id = "soda", .display_name = "Soda Wasser", .boost_category = BoostCategory::reducible});
    auto &&lime_juice = ingredient_store->add_ingredient(
        Ingredient{.id = "lime_juice", .display_name = "Limettensaft", .boost_category = BoostCategory::reducible});
    auto &&vodka = ingredient_store->add_ingredient(
        Ingredient{.id = "vodka", .display_name = "Vodka", .boost_category = BoostCategory::boostable});
    auto &&orange_juice = ingredient_store->add_ingredient(
        Ingredient{.id = "orange_juice", .display_name = "Orangensaft", .boost_category = BoostCategory::reducible});

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
        vodka.id,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre,
                                                       (0.1 * units::si::litre) / (1 * units::si::second)));
    execution_context->liquid_registry().register_dispenser(
        lime_juice.id,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre,
                                                       (0.1 * units::si::litre) / (1 * units::si::second)));
    execution_context->liquid_registry().register_dispenser(
        orange_juice.id,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre,
                                                       (0.1 * units::si::litre) / (1 * units::si::second)));

    auto only_water =
        cm::make_recipe()
            .with_name("Wasser")
            .with_description("Klassisches Wasser ohne Schickschnack")
            .with_steps()
            .with_step(std::make_unique<cm::DispenseLiquidCmd>(
                water.id, 250 * units::si::milli<units::si::litre>, generate_unique_command_id()))
            .with_step(std::make_unique<cm::DispenseLiquidCmd>(
                lime_juice.id, 30 * mp_units::si::milli<mp_units::si::litre>, generate_unique_command_id()))
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

    auto screwdriver =
        cm::make_recipe()
            .with_name("Screwdriver")
            .with_description(
                "Der Screwdriver ist ein erfrischender Cocktail aus Wodka und Orangensaft, der " // codespell:ignore
                "durch "                                                                         // codespell:ignore
                "seine einfache Zubereitung und fruchtige Note besticht.")                       // codespell:ignore
            .with_steps()
            .with_step(std::make_unique<cm::DispenseLiquidCmd>(
                vodka.id, 100 * units::si::milli<units::si::litre>, generate_unique_command_id()))
            .with_step(std::make_unique<cm::DispenseLiquidCmd>(
                orange_juice.id, 150 * mp_units::si::milli<mp_units::si::litre>, generate_unique_command_id()))
            .add()
            .with_steps()
            .with_step(std::make_unique<cm::ManualCmd>("2 Eiswürfel", generate_unique_command_id()))
            .add()
            .create();
    recipe_store->add_recipe(std::move(only_water));
    recipe_store->add_recipe(std::move(mojito));
    recipe_store->add_recipe(std::move(screwdriver));

    auto &&app_state = engine.singletonInstance<cm::app::ApplicationState *>("CocktailMaker.App", "ApplicationState");
    Q_ASSERT(app_state != nullptr);
    app_state->recipe_store = recipe_store;
    app_state->ingredient_store = ingredient_store;
    app_state->recipe_factory = recipe_factory;
    app_state->recipe_executor = recipe_executor;
    engine.loadFromModule("CocktailMaker.App", "Main");

    return app.exec();
}
