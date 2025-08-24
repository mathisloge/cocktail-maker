file(
    GENERATE OUTPUT
    application_path.hpp
    INPUT application_path.in.hpp
    TARGET CocktailMakerApp
)

add_executable(app_tests test_ui_cocktail_selection.cpp)
target_include_directories(app_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(
    app_tests
    PRIVATE Catch2::Catch2WithMain quite::quite_test Boost::process
)
catch_discover_tests(app_tests)
