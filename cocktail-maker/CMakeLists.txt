add_library(cocktail-maker-lib STATIC)
add_library(cm::lib ALIAS cocktail-maker-lib)
add_sanitizers(cocktail-maker-lib)
target_compile_features(cocktail-maker-lib PUBLIC cxx_std_26)
set_target_properties(
    cocktail-maker-lib
    PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON
)

target_link_libraries(
    cocktail-maker-lib
    PUBLIC cm::core
    PRIVATE
        simdjson::simdjson
        $<$<TARGET_EXISTS:PkgConfig::libudev>:PkgConfig::libudev>
)

target_sources(
    cocktail-maker-lib
    PUBLIC
        FILE_SET CXX_MODULES
            FILES
                async_machine_protocol_server.cppm
                async_machine_protocol_server.cpp
                dispenser.cppm
                execute_recipe.cppm
                glass.cppm
                glass.cpp
                ingredient.cppm
                ingredient.cpp
                lib.cppm
                pod_discovery.cppm
                pod_types.cppm
                pod_registry.cppm
                pod_dispatcher.cppm
                pod.cppm
                pod.cpp
                pod_algorithms.cppm
                recipe_boost.cppm
                recipe_scale.cppm
                recipe.cppm
                recipe.cpp
                station_config.cppm
                station_config.cpp
                station_state.cppm
                serial.cppm
                serial.cpp
                application.cppm
                application.cpp
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
            async_machine_protocol_server.test.cpp
            execute_recipe.test.cpp
            glass.test.cpp
            ingredient.test.cpp
            pod_discovery.test.cpp
            pod.test.cpp
            pod_registry.test.cpp
            pod_algorithms.test.cpp
            recipe_boost.test.cpp
            recipe_scale.test.cpp
            recipe.test.cpp
    )
endif()
