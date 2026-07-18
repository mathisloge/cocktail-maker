add_library(generated-ui STATIC)
add_library(cm::ui ALIAS generated-ui)
add_sanitizers(generated-ui)
target_compile_features(generated-ui PUBLIC cxx_std_26)
target_link_libraries(generated-ui PUBLIC Slint::Slint)
slint_target_sources(generated-ui
  ui/app-window.slint NAMESPACE cm::gui
)
target_include_directories(generated-ui PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

add_library(cocktail-maker-gui STATIC)
add_library(cm::gui ALIAS cocktail-maker-gui)
add_sanitizers(cocktail-maker-gui)
target_compile_features(cocktail-maker-gui PUBLIC cxx_std_26)
set_target_properties(
    cocktail-maker-gui
    PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON
)
target_link_libraries(cocktail-maker-gui PUBLIC cm::ui cm::lib Boost::hash2)
target_sources(
    cocktail-maker-gui
    PUBLIC
        FILE_SET CXX_MODULES
            FILES
                src/lib.cppm
                src/recipe_adapter.cppm
                src/async_ui.cppm
                src/maschine_adapter.cppm
                src/ui_log_sink.cppm
                src/station_state_bridge.cppm
                src/station_state_bridge.cpp
                src/dispenser_calibration_bridge.cppm
                src/dispenser_calibration_bridge.cpp
                src/recipe_context_bridge.cppm
                src/recipe_context_bridge.cpp
                src/process_context_bridge.cppm
                src/process_context_bridge.cpp
                src/glass_context_bridge.cppm
                src/gui_application.cppm
                src/gui_application.cpp
                src/operator_auth.cppm
)

if(BUILD_TESTING)
    add_executable(cocktail-maker-gui-tests)
    add_sanitizers(cocktail-maker-gui-tests)
    target_link_libraries(
        cocktail-maker-gui-tests
        PRIVATE Catch2::Catch2WithMain cm::gui
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
    )
endif()

add_executable(cocktail-maker src/main.cpp)
add_sanitizers(cocktail-maker)
target_link_libraries(cocktail-maker PRIVATE cm::gui cocktail-maker-simulator)
target_include_directories(
    cocktail-maker
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR} @ONLY
)
configure_file(src/config.in.hpp config.hpp)

file(
    COPY "${PROJECT_SOURCE_DIR}/db"
    DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
