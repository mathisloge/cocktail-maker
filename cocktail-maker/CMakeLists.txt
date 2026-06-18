add_library(cocktail-maker-lib STATIC)
add_library(cm::lib ALIAS cocktail-maker-lib)
add_sanitizers(cocktail-maker-lib)
target_compile_features(cocktail-maker-lib PUBLIC cxx_std_26)

target_link_libraries(
    cocktail-maker-lib
    PUBLIC
        mp-units::mp-units
        spdlog::spdlog
        simdjson::simdjson
        Boost::cobalt
        libassert::assert
    PRIVATE proto
)

target_sources(
    cocktail-maker-lib
    PUBLIC
        FILE_SET CXX_MODULES
            FILES
                async_machine_protocol_server.cppm
                awaitable_bool.cppm
                comms_adapter.cppm
                dispense_command.cppm
                dispenser.cppm
                execute_recipe.cppm
                ingredient.cppm
                lib.cppm
                logging.cppm
                overloaded.cppm
                pod_discovery.cppm
                pod_types.cppm
                pod_registry.cppm
                pod_dispatcher.cppm
                pod.cppm
                recipe_boost.cppm
                recipe.cppm
                station_config.cppm
                station_state.cppm
                strong_type.cppm
                units.cppm
)

if(BUILD_TESTING)
    add_executable(cocktail-maker-lib-tests)
    add_sanitizers(cocktail-maker-lib-tests)
    target_link_libraries(
        cocktail-maker-lib-tests
        PRIVATE Catch2::Catch2WithMain cm::lib proto
    )
    catch_discover_tests(cocktail-maker-lib-tests)

    target_sources(
        cocktail-maker-lib-tests
        PRIVATE
            async_machine_interface.test.cpp
            async_machine_protocol_server.test.cpp
            awaitable_bool.test.cpp
            dispense_command.test.cpp
            comms_adapter.test.cpp
            execute_recipe.test.cpp
            ingredient.test.cpp
            pod_discovery.test.cpp
            pod.test.cpp
            pod_registry.test.cpp
            recipe_boost.test.cpp
            recipe.test.cpp
            strong_type.test.cpp
            station_config.test.cpp
    )
endif()
