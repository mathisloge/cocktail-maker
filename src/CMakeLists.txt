add_library(cocktail-maker-core STATIC)
add_library(cm::core ALIAS cocktail-maker-core)
target_compile_features(cocktail-maker-core PUBLIC cxx_std_23)
set_target_properties(
    cocktail-maker-core
    PROPERTIES POSITION_INDEPENDENT_CODE ON VISIBILITY_INLINES_HIDDEN ON
)

target_link_libraries(
    cocktail-maker-core
    PUBLIC
        PkgConfig::libgpiodcxx # order is important here!
        PkgConfig::libgpiod # order is important here!
        Boost::asio
        mp-units::core
        mp-units::systems
        mp-units::mp-units
        mp-units::mp-units-contracts
        fmt::fmt
)

target_sources(
    cocktail-maker-core
    PUBLIC
        FILE_SET headers
            TYPE HEADERS
            BASE_DIRS include
            FILES
                include/cm/hw/pin_selection.hpp
                include/cm/hw/hx711_sensor.hpp
                include/cm/hw/motor.hpp
                include/cm/hw/stepper_motor.hpp
                include/cm/hw/drv8825_stepper_moter.hpp
    PRIVATE src/hw/hx711_sensor.cpp src/hw/drv8825_stepper_moter.cpp
)
