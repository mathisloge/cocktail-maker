# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later
find_package(ftxui CONFIG REQUIRED)

add_executable(calibration main.cpp)
target_link_libraries(
    calibration
    PRIVATE cm::core ftxui::dom ftxui::screen ftxui::component
)
