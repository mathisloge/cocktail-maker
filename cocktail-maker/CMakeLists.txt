add_library(cocktail-maker-lib)
add_library(cm::lib ALIAS cocktail-maker-lib)
target_compile_features(cocktail-maker-lib PUBLIC cxx_std_26)

target_link_libraries(
    cocktail-maker-lib
    PUBLIC mp-units::mp-units spdlog::spdlog simdjson::simdjson Boost::cobalt
)

target_sources(
    cocktail-maker-lib
    PUBLIC
        FILE_SET CXX_MODULES
            FILES
                lib.cppm
                logging.cppm
                units.cppm
                ingredient.cppm
                recipe.cppm
                recipe_boost.cppm
                execute_recipe.cppm
)

if(BUILD_TESTING)
    add_executable(cocktail-maker-lib-tests)
    target_link_libraries(
        cocktail-maker-lib-tests
        PRIVATE Catch2::Catch2WithMain cm::lib
    )
    catch_discover_tests(cocktail-maker-lib-tests)

    target_sources(
        cocktail-maker-lib-tests
        PRIVATE recipe.test.cpp ingredient.test.cpp recipe_boost.test.cpp
    )
endif()
