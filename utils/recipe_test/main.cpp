// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <boost/asio.hpp>
#include <cm/commands/dispense_liquid_cmd.hpp>
#include <cm/execution_context.hpp>
#include <cm/hw/drv8825_stepper_moter.hpp>
#include <cm/liquid_dispenser_stepper_pump.hpp>
#include <cm/logging.hpp>
#include <cm/recipe.hpp>
#include <cm/recipe_executor.hpp>
#include <spdlog/spdlog.h>
#include "cm/hw/hx711_sensor.hpp"

int main()
{
    boost::asio::io_context io;
    auto weight_sensor = std::make_unique<cm::Hx711Sensor>(io.get_executor(),
                                                           cm::Hx711DatPin{.chip = "/dev/gpiochip0", .offset = {23}},
                                                           cm::Hx711ClkPin{.chip = "/dev/gpiochip0", .offset = {24}});

    auto liquid_dispenser = std::make_unique<cm::StepperPumpLiquidDispenser>(
        "test-dispenser",
        std::make_unique<cm::Drv8825StepperMotorDriver>(cm::Drv8825EnablePin{.chip = "/dev/gpiochip0", .offset = {17}},
                                                        cm::Drv8825StepPin{.chip = "/dev/gpiochip0", .offset = {27}},
                                                        cm::Drv8825DirectionPin{.chip = "/dev/gpiochip0", .offset = {22}}),
        1 * cm::units::si::litre,
        (1000 * cm::units::step) / (100 * cm::units::milli_litre),
        52 * cm::units::milli_litre);

    std::shared_ptr<cm::ExecutionContext> ctx =
        std::make_shared<cm::ExecutionContext>(io.get_executor(), std::move(weight_sensor));
    ctx->liquid_registry().register_dispenser("water", std::move(liquid_dispenser));

    ctx->event_bus().subscribe([logger = cm::LoggingContext::instance().create_logger("Events"), ctx](auto&& event) {
        SPDLOG_LOGGER_DEBUG(logger, "Received event {}", event);

        if (std::holds_alternative<cm::RefillIngredientEvent>(event)) {
            SPDLOG_LOGGER_DEBUG(logger, "Resume... {}", event);
            boost::asio::post([ctx]() { ctx->resume(); });
        }
    });

    auto recipe = cm::make_recipe()
                      .with_name("Only Water")
                      .with_steps()
                      .with_step(std::make_unique<cm::DispenseLiquidCmd>(
                          "water", 250 * cm::units::milli_litre, cm::generate_unique_command_id()))
                      .add()
                      .create();
    cm::RecipeExecutor recipe_executor{ctx, recipe};
    recipe_executor.run();

    std::vector<std::thread> threads;

    for (int i = 0; i < 1; i++) {
        threads.emplace_back(std::thread{[&io]() { io.run(); }});
    }
    io.run();
    return 0;
}
