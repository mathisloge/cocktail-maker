# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(drv8825_test main.cpp)
target_link_libraries(drv8825_test PRIVATE cm::core)
