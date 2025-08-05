add_library(cocktail-maker-core STATIC)
add_library(cm::core ALIAS cocktail-maker-core)
target_compile_features(cocktail-maker-core PUBLIC cxx_std_23)
set_target_properties(
    cocktail-maker-core
    PROPERTIES POSITION_INDEPENDENT_CODE ON VISIBILITY_INLINES_HIDDEN ON
)

target_compile_definitions(
    cocktail-maker-core
    PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
)

target_link_libraries(
    cocktail-maker-core
    PUBLIC
        PkgConfig::libgpiodcxx # order is important here!
        PkgConfig::libgpiod # order is important here!
        Boost::asio
        Boost::signals2
        mp-units::core
        mp-units::systems
        mp-units::mp-units
        mp-units::mp-units-contracts
        spdlog::spdlog
        fmt::fmt
)

target_sources(
    cocktail-maker-core
    PUBLIC
        FILE_SET headers
            TYPE HEADERS
            BASE_DIRS include
            FILES
                include/cm/overloads.hpp
                include/cm/logging.hpp
                include/cm/liquid_dispenser_registry.hpp
                include/cm/liquid_dispenser_stepper_pump.hpp
                include/cm/liquid_dispenser_simulated.hpp
                include/cm/machine_state.hpp
                include/cm/execution_context.hpp
                include/cm/recipe.hpp
                include/cm/recipe_store.hpp
                include/cm/recipe_executor.hpp
                include/cm/units.hpp
                include/cm/ingredient_id.hpp
                include/cm/commands/command.hpp
                include/cm/commands/dispense_liquid_cmd.hpp
                include/cm/commands/manual_cmd.hpp
                include/cm/events/event_bus.hpp
                include/cm/events/execution_canceled_event.hpp
                include/cm/events/manual_action_event.hpp
                include/cm/events/refill_ingredient_event.hpp
                include/cm/events/recipe_finished.hpp
                include/cm/hw/pin_selection.hpp
                include/cm/hw/hx711_sensor.hpp
                include/cm/hw/motor.hpp
                include/cm/hw/stepper_motor.hpp
                include/cm/hw/drv8825_stepper_moter.hpp
    PRIVATE
        src/logging.cpp
        src/hw/hx711_sensor.cpp
        src/hw/drv8825_stepper_moter.cpp
        src/execution_context.cpp
        src/liquid_dispenser_registry.cpp
        src/liquid_dispenser_simulated.cpp
        src/liquid_dispenser_stepper_pump.cpp
        src/machine_state.cpp
        src/recipe_executor.cpp
        src/recipe.cpp
        src/recipe_store.cpp
        src/events/event_bus.cpp
        src/commands/dispense_liquid_cmd.cpp
        src/commands/manual_cmd.cpp
)
