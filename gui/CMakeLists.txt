add_library(generated-ui STATIC)
add_library(cm::ui ALIAS generated-ui)
add_sanitizers(generated-ui)
target_compile_features(generated-ui PUBLIC cxx_std_26)
target_link_libraries(generated-ui PUBLIC Slint::Slint)
# The MCP server can only name elements and their ids when the compiler emits debug info.
if(CM_SLINT_MCP)
    set(SLINT_COMPILER_ENV_VARS SLINT_EMIT_DEBUG_INFO=1)
endif()
slint_target_sources(generated-ui
  ui/slint-project.json NAMESPACE cm::gui
)
target_include_directories(generated-ui PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

add_library(cocktail-maker-gui STATIC)
add_library(cm::gui ALIAS cocktail-maker-gui)
add_sanitizers(cocktail-maker-gui)
cm_enable_compile_time_trace(cocktail-maker-gui)
target_compile_features(cocktail-maker-gui PUBLIC cxx_std_26)
set_target_properties(
    cocktail-maker-gui
    PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON
)
target_link_libraries(cocktail-maker-gui PUBLIC cm::ui cm::lib Boost::hash2)
target_sources(
    cocktail-maker-gui
    PRIVATE
        src/dispenser_calibration_bridge.cpp
        src/gui_application.cpp
        src/maschine_adapter.cpp
        src/mix_error_page.cpp
        src/process_context_bridge.cpp
        src/recipe_context_bridge.cpp
        src/station_state_bridge.cpp
    PUBLIC
        FILE_SET CXX_MODULES
            FILES
                src/lib.cppm
                src/recipe_adapter.cppm
                src/async_ui.cppm
                src/maschine_adapter.cppm
                src/ui_log_sink.cppm
                src/station_state_bridge.cppm
                src/dispenser_calibration_bridge.cppm
                src/recipe_context_bridge.cppm
                src/process_context_bridge.cppm
                src/mix_error_page.cppm
                src/glass_context_bridge.cppm
                src/gui_application.cppm
                src/operator_auth.cppm
)

if(BUILD_TESTING)
    add_executable(cocktail-maker-gui-tests)
    set_target_properties(
        cocktail-maker-gui-tests
        PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON
    )
    add_sanitizers(cocktail-maker-gui-tests)
    cm_enable_compile_time_trace(cocktail-maker-gui-tests)
    target_link_libraries(
        cocktail-maker-gui-tests
        PRIVATE
            Catch2::Catch2WithMain
            cm::gui
            cocktail-maker-simulator
            cm::testing
            proto
    )
    catch_discover_tests(cocktail-maker-gui-tests)
    target_compile_definitions(
        cocktail-maker-gui-tests
        PRIVATE TEST_IMAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test_assets"
    )

    target_sources(
        cocktail-maker-gui-tests
        PRIVATE
            src/recipe_adapter.test.cpp
            src/async_ui.test.cpp
            src/station_state_bridge.test.cpp
            src/operator_auth.test.cpp
            src/maschine_adapter.test.cpp
            src/station_flows.test.cpp
            src/dispenser_calibration_bridge.test.cpp
            src/mix_error_page.test.cpp
            src/ui_snapshots.test.cpp
    )
endif()

add_executable(cocktail-maker src/main.cpp)
add_sanitizers(cocktail-maker)
cm_enable_compile_time_trace(cocktail-maker)
target_link_libraries(
    cocktail-maker
    PRIVATE cm::gui cocktail-maker-simulator CLI11::CLI11
)
target_include_directories(
    cocktail-maker
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR} @ONLY
)
configure_file(src/config.in.hpp config.hpp)

file(
    COPY "${PROJECT_SOURCE_DIR}/db"
    DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
