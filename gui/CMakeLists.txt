add_library(cocktail-maker-gui)
add_library(cm::gui ALIAS cocktail-maker-gui)
add_sanitizers(cocktail-maker-gui)
target_compile_features(cocktail-maker-gui PUBLIC cxx_std_26)
target_link_libraries(cocktail-maker-gui PUBLIC Slint::Slint cm::lib)
slint_target_sources(cocktail-maker-gui
  ui/app-window.slint NAMESPACE cm::gui
)
target_include_directories(
    cocktail-maker-gui
    PUBLIC "${CMAKE_CURRENT_BINARY_DIR}"
)
target_sources(
    cocktail-maker-gui
    PUBLIC
        FILE_SET CXX_MODULES
            FILES
                src/lib.cppm
                src/recipe_adapter.cppm
                src/async_ui.cppm
                src/maschine_adapter.cppm
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
        PRIVATE src/recipe_adapter.test.cpp src/async_ui.test.cpp
    )
endif()

add_executable(cocktail-maker src/main.cpp)
add_sanitizers(cocktail-maker)
target_link_libraries(cocktail-maker PRIVATE cm::gui)
