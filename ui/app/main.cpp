// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <CLI/App.hpp>
#include <QApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QQuickWindow>
#include <cm/config.hpp>
#include <cm/db/db.hpp>
#include <cm/execution_context.hpp>
#include <cm/glass_store.hpp>
#include <cm/hw/weight_sensor_simulated.hpp>
#include <cm/hw/wled_serial.hpp>
#include <cm/led_state_manager.hpp>
#include <cm/liquid_dispenser_simulated.hpp>
#include <cm/recipe.hpp>
#include <cm/recipe_store.hpp>
#include <mp-units/systems/imperial.h>
#include <mp-units/systems/international.h>
#include "ApplicationState.hpp"

Q_IMPORT_QML_PLUGIN(CocktailMaker_UiPlugin)
using namespace cm;

int main(int argc, char* argv[])
{
    CLI::App cli{std::string{kGitRef}, "CocktailMaker"};
    argv = cli.ensure_utf8(argv);

    bool fullscreen{false};
    size_t threads{3};

    cli.add_option("--threads", threads, "Number of threads to use");
    cli.add_flag("--fullscreen", fullscreen, "Set if the app should start in fullscreen mode.");
    cli.set_version_flag("--version", std::string{kVersion});

    CLI11_PARSE(cli, argc, argv);

    boost::asio::thread_pool thread_pool{threads};
    boost::asio::cancellation_signal cancel_signal;

    QApplication app{argc, argv};
    QCoreApplication::setApplicationName(QStringLiteral("CocktailMaker"));
    QCoreApplication::setOrganizationName(QStringLiteral("com.mathisloge.cocktail-maker"));
    QCoreApplication::setApplicationVersion(QString::fromLocal8Bit(kVersion));

    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    app.setPalette(palette);

    QQmlApplicationEngine engine;

    engine.setInitialProperties(
        QVariantMap{{"visibility", fullscreen ? QWindow::Visibility::FullScreen : QWindow::Visibility::Windowed}});

    std::shared_ptr<cm::ExecutionContext> execution_context = std::make_shared<cm::ExecutionContext>(thread_pool.get_executor());
    auto weight_sensor = std::make_shared<WeightSensorSimulated>();
    weight_sensor->start_measure_loop(thread_pool.get_executor(), cancel_signal.slot());
    auto ingredient_store = std::make_shared<cm::IngredientStore>();
    auto recipe_store = std::make_shared<cm::RecipeStore>();
    auto glass_store = std::make_shared<cm::GlassStore>();
    auto recipe_factory = std::make_shared<cm::ui::RecipeFactory>(recipe_store, ingredient_store);
    auto recipe_executor = std::make_shared<cm::ui::RecipeExecutorAdapter>(execution_context, ingredient_store, glass_store);
    auto leds = WledSerial::create(thread_pool.get_executor(), cancel_signal.slot(), "/dev/ttyUSB0", {{0, 24}});
    auto led_manager = std::make_shared<cm::LedStateManager>(leds);
    led_manager->subscribe_to_events(execution_context->event_bus());
    led_manager->connect_led_segment_with_ingredient(0, db::vodka);

    db::register_ingredients(*ingredient_store);

    execution_context->liquid_registry().register_dispenser(
        db::water,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre,
                                                       (0.1 * units::si::litre) / (0.5 * units::si::second)));
    execution_context->liquid_registry().register_dispenser(
        db::bacardi,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre,
                                                       (0.1 * units::si::litre) / (0.5 * units::si::second)));
    execution_context->liquid_registry().register_dispenser(
        db::soda,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre, (0.1 * units::si::litre) / (2 * units::si::second)));
    execution_context->liquid_registry().register_dispenser(
        db::vodka,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre, (0.1 * units::si::litre) / (1 * units::si::second)));
    execution_context->liquid_registry().register_dispenser(
        db::lime_juice,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre, (0.1 * units::si::litre) / (1 * units::si::second)));
    execution_context->liquid_registry().register_dispenser(
        db::orange_juice,
        std::make_unique<cm::SimulatedLiquidDispenser>(1 * units::si::litre, (0.1 * units::si::litre) / (1 * units::si::second)));

    db::register_possible_recipes(*execution_context, *recipe_store);

    glass_store->add_glass(Glass{.id = "g1", .display_name = "Glass1", .capacity = 0.25 * units::si::litre},
                           500 * units::si::gram);
    glass_store->add_glass(Glass{.id = "g2", .display_name = "Glass2", .capacity = 0.1 * units::si::litre},
                           150 * units::si::gram);
    glass_store->add_glass(Glass{.id = "g3", .display_name = "Glass3", .capacity = 0.4 * units::si::litre},
                           600 * units::si::gram);

    auto&& app_state = engine.singletonInstance<cm::app::ApplicationState*>("CocktailMaker.App", "ApplicationState");
    Q_ASSERT(app_state != nullptr);
    app_state->recipe_store = recipe_store;
    app_state->ingredient_store = ingredient_store;
    app_state->glass_store = glass_store;
    app_state->recipe_factory = recipe_factory;
    app_state->recipe_executor = recipe_executor;

    engine.loadFromModule("CocktailMaker.App", "Main");

#if 0 // NOLINT
    engine.loadFromModule("CocktailMaker.App", "DebugWindow");
#endif

    const auto res = app.exec();

    // shutdown section
#undef emit
    leds->turn_off();
    boost::asio::post(thread_pool, [&cancel_signal]() { cancel_signal.emit(boost::asio::cancellation_type::all); });
    return res;
}
