# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(
    core_tests
    test_glass_store.cpp
    test_recipe.cpp
    test_recipe_booster.cpp
)
target_link_libraries(core_tests PRIVATE Catch2::Catch2WithMain cm::core)
catch_discover_tests(core_tests)
