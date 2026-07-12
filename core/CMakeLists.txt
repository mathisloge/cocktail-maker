add_library(cocktail-maker-core STATIC)
add_library(cm::core ALIAS cocktail-maker-core)
add_sanitizers(cocktail-maker-core)
target_compile_features(cocktail-maker-core PUBLIC cxx_std_26)
set_target_properties(
    cocktail-maker-core
    PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON
)

target_compile_definitions(
    cocktail-maker-core
    PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
)

target_link_libraries(
    cocktail-maker-core
    PUBLIC
        mp-units::mp-units
        spdlog::spdlog
        simdjson::simdjson
        Boost::cobalt
        libassert::assert
        proto
)

target_sources(
    cocktail-maker-core
    PUBLIC
        FILE_SET CXX_MODULES
            FILES
                lib.cppm
                any_io_stream.cppm
                awaitable_bool.cppm
                comms_adapter.cppm
                logging.cppm
                overloaded.cppm
                strong_type.cppm
                units.cppm
)

if(BUILD_TESTING)
    add_executable(cocktail-maker-core-tests)
    add_sanitizers(cocktail-maker-core-tests)
    target_link_libraries(
        cocktail-maker-core-tests
        PRIVATE Catch2::Catch2WithMain cm::core proto
    )
    catch_discover_tests(cocktail-maker-core-tests)

    target_sources(
        cocktail-maker-core-tests
        PRIVATE
            awaitable_bool.test.cpp
            comms_adapter.test.cpp
            strong_type.test.cpp
    )
endif()
