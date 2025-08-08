# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(recipe_test main.cpp)
target_link_libraries(recipe_test PRIVATE cm::core)
