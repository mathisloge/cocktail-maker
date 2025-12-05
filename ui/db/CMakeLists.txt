# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_qml_module(cocktail-maker-db
    STATIC
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CocktailMaker/Db"
    URI "CocktailMaker.Db"
    RESOURCES
        assets/mojito.png
        assets/screwdriver.png
        assets/tequila-sunrise.png
        assets/moscow_mule.png
        assets/virgin_sunrise.png
        assets/passion_rum_cooler.png
)
add_library(cm::db ALIAS cocktail-maker-db)
target_link_libraries(cocktail-maker-db PUBLIC cm::core)

target_sources(
    cocktail-maker-db
    PUBLIC
        FILE_SET headers
            TYPE HEADERS
            BASE_DIRS include
            FILES include/cm/db/db.hpp
    PRIVATE
        src/db.cpp
        src/recipes/mojito.hpp
        src/recipes/moscow_mule.hpp
        src/recipes/passion_rum_cooler.hpp
        src/recipes/screwdriver.hpp
        src/recipes/tequila_sunrise.hpp
        src/recipes/virgin_sunrise.hpp
)
