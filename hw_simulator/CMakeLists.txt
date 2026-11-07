add_library(cocktail-maker-simulator STATIC)
add_sanitizers(cocktail-maker-simulator)
target_link_libraries(cocktail-maker-simulator PUBLIC cm::lib PRIVATE proto)
target_sources(
    cocktail-maker-simulator
    PUBLIC
        FILE_SET CXX_MODULES
            FILES
                lib.cppm
                client.cppm
                simulated_pod_discovery.cppm
                simulated_pod_discovery.cpp
)

if(BUILD_TESTING)
    add_executable(cocktail-maker-sim-tests)
    add_sanitizers(cocktail-maker-sim-tests)
    target_link_libraries(
        cocktail-maker-sim-tests
        PRIVATE
            Catch2::Catch2WithMain
            cocktail-maker-simulator
            cm::testing
            proto
    )
    catch_discover_tests(cocktail-maker-sim-tests)

    target_sources(
        cocktail-maker-sim-tests
        PRIVATE client.test.cpp pod_simulator.test.cpp
    )
endif()
