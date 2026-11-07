add_library(cocktail-maker-testing STATIC)
add_library(cm::testing ALIAS cocktail-maker-testing)
add_sanitizers(cocktail-maker-testing)
cm_enable_compile_time_trace(cocktail-maker-testing)
target_compile_features(cocktail-maker-testing PUBLIC cxx_std_26)
set_target_properties(
    cocktail-maker-testing
    PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON
)

target_link_libraries(cocktail-maker-testing PUBLIC cm::core)

target_sources(
    cocktail-maker-testing
    PUBLIC
        FILE_SET CXX_MODULES
            FILES lib.cppm run_until_complete.cppm temp_dir.cppm
)
