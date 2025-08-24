# SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
#
# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_qml_module(cocktail-maker-ui
    STATIC
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CocktailMaker/Ui"
    URI "CocktailMaker.Ui"
    QML_FILES
        qml/ImageCard.qml
        qml/Slider.qml
        qml/Button.qml
        qml/Section.qml
        qml/ManualCommandPopup.qml
    RESOURCES
        mojito.png
)
add_library(cm::ui ALIAS cocktail-maker-ui)
target_link_libraries(cocktail-maker-ui PUBLIC cm::core quite::probe)

target_sources(
    cocktail-maker-ui
    PUBLIC
        FILE_SET headers
            TYPE HEADERS
            BASE_DIRS include
            FILES
                include/cm/ui/GlassAdapter.hpp
                include/cm/ui/GlassListModel.hpp
                include/cm/ui/GlassStoreForeign.hpp
                include/cm/ui/IngredientStoreForeign.hpp
                include/cm/ui/RecipeBoosterAdapter.hpp
                include/cm/ui/RecipeCommandStatusModel.hpp
                include/cm/ui/RecipeDetail.hpp
                include/cm/ui/RecipeExecutorAdapter.hpp
                include/cm/ui/RecipeFactory.hpp
                include/cm/ui/RecipeListModel.hpp
                include/cm/ui/RecipeStoreForeign.hpp
    PRIVATE
        src/GlassListModel.cpp
        src/RecipeBoosterAdapter.cpp
        src/RecipeCommandStatusModel.cpp
        src/RecipeDetail.cpp
        src/RecipeExecutorAdapter.cpp
        src/RecipeFactory.cpp
        src/RecipeListModel.cpp
)
# needed for moc
target_include_directories(
    cocktail-maker-ui
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/cm/ui
)
