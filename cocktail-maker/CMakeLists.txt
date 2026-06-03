add_library(cocktail-maker-lib)
add_library(cm::lib ALIAS cocktail-maker-lib)
add_sanitizers(cocktail-maker-lib)
target_compile_features(cocktail-maker-lib PUBLIC cxx_std_26)

target_link_libraries(
    cocktail-maker-lib
    PUBLIC mp-units::mp-units spdlog::spdlog simdjson::simdjson Boost::cobalt
    PRIVATE proto
)

target_sources(
    cocktail-maker-lib
    PUBLIC
        FILE_SET CXX_MODULES
            FILES
                execute_recipe.cppm
                ingredient.cppm
                lib.cppm
                logging.cppm
                machine_protocol.cppm
                recipe_boost.cppm
                recipe.cppm
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
            execute_recipe.test.cpp
            ingredient.test.cpp
            machine_protocol.test.cpp
            recipe_boost.test.cpp
            recipe.test.cpp
    )
endif()
