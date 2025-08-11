add_executable(core_tests test_glass_store.cpp)
target_link_libraries(core_tests PRIVATE Catch2::Catch2WithMain cm::core)
catch_discover_tests(core_tests)
