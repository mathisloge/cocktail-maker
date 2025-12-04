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
#include "cm/hw/drv8825_stepper_moter.hpp"
#include "cm/liquid_dispenser_stepper_pump.hpp"

Q_IMPORT_QML_PLUGIN(CocktailMaker_UiPlugin)
using namespace cm;

// Port A (GPA0-GPA7)
constexpr auto kPA0 = gpiod::line::offset{0};
constexpr auto kPA1 = gpiod::line::offset{1};
constexpr auto kPA2 = gpiod::line::offset{2};
constexpr auto kPA3 = gpiod::line::offset{3};
constexpr auto kPA4 = gpiod::line::offset{4};
constexpr auto kPA5 = gpiod::line::offset{5};
constexpr auto kPA6 = gpiod::line::offset{6};
constexpr auto kPA7 = gpiod::line::offset{7};

// Port B (GPB0-GPB7)
constexpr auto kPB0 = gpiod::line::offset{8};
constexpr auto kPB1 = gpiod::line::offset{9};
constexpr auto kPB2 = gpiod::line::offset{10};
constexpr auto kPB3 = gpiod::line::offset{11};
constexpr auto kPB4 = gpiod::line::offset{12};
constexpr auto kPB5 = gpiod::line::offset{13};
constexpr auto kPB6 = gpiod::line::offset{14};
constexpr auto kPB7 = gpiod::line::offset{15};

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

    if (fullscreen) {
        app.setOverrideCursor(Qt::BlankCursor);
    }
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
    auto execution_context_adapter = std::make_shared<cm::ui::ExecutionContextAdapter>(execution_context);
    auto leds = WledSerial::create(thread_pool.get_executor(), cancel_signal.slot(), "/dev/ttyUSB0", {{0, 24}});
    auto led_manager = std::make_shared<cm::LedStateManager>(leds);
    led_manager->subscribe_to_events(execution_context->event_bus());
    led_manager->connect_led_segment_with_ingredient(0, db::vodka);

    db::register_ingredients(*ingredient_store);

    auto pump1 = std::make_unique<StepperPumpLiquidDispenser>(
        "pump1",
        std::make_unique<Drv8825StepperMotorDriver>("pump1",
                                                    cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {17}},
                                                    cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {27}},
                                                    cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {22}}),
        1 * units::si::litre,
        (5000 * cm::units::step) / cm::units::si::litre,
        500 * units::milli_litre);

    auto pump2 = std::make_unique<StepperPumpLiquidDispenser>(
        "pump2",
        std::make_unique<Drv8825StepperMotorDriver>("pump2",
                                                    cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {11}},
                                                    cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {9}},
                                                    cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {10}}),
        1 * units::si::litre,
        (5000 * cm::units::step) / cm::units::si::litre,
        500 * units::milli_litre);
    auto pump3 = std::make_unique<StepperPumpLiquidDispenser>(
        "pump3",
        std::make_unique<Drv8825StepperMotorDriver>("pump3",
                                                    cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {13}},
                                                    cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {6}},
                                                    cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {5}}),
        1 * units::si::litre,
        (161290 * cm::units::step) / cm::units::si::litre,
        500 * units::milli_litre);

    auto pump4 = std::make_unique<StepperPumpLiquidDispenser>(
        "pump4",
        std::make_unique<Drv8825StepperMotorDriver>("pump4",
                                                    cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {21}},
                                                    cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {20}},
                                                    cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {16}}),
        1 * units::si::litre,
        (5000 * cm::units::step) / cm::units::si::litre,
        500 * units::milli_litre);

    auto pump5 = std::make_unique<StepperPumpLiquidDispenser>(
        "pump5",
        std::make_unique<Drv8825StepperMotorDriver>("pump5",
                                                    cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {25}},
                                                    cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {8}},
                                                    cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {7}}),
        1 * units::si::litre,
        (5000 * cm::units::step) / cm::units::si::litre,
        500 * units::milli_litre);

    auto pump6 = std::make_unique<StepperPumpLiquidDispenser>(
        "pump6",
        std::make_unique<Drv8825StepperMotorDriver>("pump6",
                                                    cm::Drv8825EnablePin{.chip = "/dev/gpiochip14", .offset = kPB2},
                                                    cm::Drv8825StepPin{.chip = "/dev/gpiochip14", .offset = kPB3},
                                                    cm::Drv8825DirectionPin{.chip = "/dev/gpiochip14", .offset = kPB4}),
        1 * units::si::litre,
        (5000 * cm::units::step) / cm::units::si::litre,
        500 * units::milli_litre);

    auto pump7 = std::make_unique<StepperPumpLiquidDispenser>(
        "pump7",
        std::make_unique<Drv8825StepperMotorDriver>("pump7",
                                                    cm::Drv8825EnablePin{.chip = "/dev/gpiochip14", .offset = kPA6},
                                                    cm::Drv8825StepPin{.chip = "/dev/gpiochip14", .offset = kPB0},
                                                    cm::Drv8825DirectionPin{.chip = "/dev/gpiochip14", .offset = kPB1}),
        1 * units::si::litre,
        (5000 * cm::units::step) / cm::units::si::litre,
        500 * units::milli_litre);

    auto pump8 = std::make_unique<StepperPumpLiquidDispenser>(
        "pump8",
        std::make_unique<Drv8825StepperMotorDriver>("pump8",
                                                    cm::Drv8825EnablePin{.chip = "/dev/gpiochip14", .offset = kPB7},
                                                    cm::Drv8825StepPin{.chip = "/dev/gpiochip14", .offset = kPB6},
                                                    cm::Drv8825DirectionPin{.chip = "/dev/gpiochip14", .offset = kPB5}),
        1 * units::si::litre,
        (5000 * cm::units::step) / cm::units::si::litre,
        500 * units::milli_litre);

    auto pump9 = std::make_unique<StepperPumpLiquidDispenser>(
        "pump9",
        std::make_unique<Drv8825StepperMotorDriver>("pump9",
                                                    cm::Drv8825EnablePin{.chip = "/dev/gpiochip14", .offset = kPA3},
                                                    cm::Drv8825StepPin{.chip = "/dev/gpiochip14", .offset = kPA4},
                                                    cm::Drv8825DirectionPin{.chip = "/dev/gpiochip14", .offset = kPA5}),
        1 * units::si::litre,
        (5000 * cm::units::step) / cm::units::si::litre,
        500 * units::milli_litre);

    auto pump10 = std::make_unique<StepperPumpLiquidDispenser>(
        "pump10",
        std::make_unique<Drv8825StepperMotorDriver>("pump10",
                                                    cm::Drv8825EnablePin{.chip = "/dev/gpiochip14", .offset = kPA0},
                                                    cm::Drv8825StepPin{.chip = "/dev/gpiochip14", .offset = kPA1},
                                                    cm::Drv8825DirectionPin{.chip = "/dev/gpiochip14", .offset = kPA2}),
        1 * units::si::litre,
        (5000 * cm::units::step) / cm::units::si::litre,
        500 * units::milli_litre);

    execution_context->liquid_registry().register_dispenser(db::water, std::move(pump1));
    execution_context->liquid_registry().register_dispenser(db::bacardi, std::move(pump2));
    execution_context->liquid_registry().register_dispenser(db::soda, std::move(pump3));
    execution_context->liquid_registry().register_dispenser(db::vodka, std::move(pump4));
    execution_context->liquid_registry().register_dispenser(db::lime_juice, std::move(pump5));
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
    app_state->execution_context_adapter = execution_context_adapter;

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
