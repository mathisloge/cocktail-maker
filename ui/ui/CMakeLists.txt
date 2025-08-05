qt_add_qml_module(cocktail-maker-ui
    STATIC
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CocktailMaker/Ui"
    URI "CocktailMaker.Ui"
    QML_FILES
        qml/ImageCard.qml
        qml/Slider.qml
        qml/Button.qml
        qml/Section.qml
    RESOURCES
        mojito.png
)
add_library(cm::ui ALIAS cocktail-maker-ui)
target_link_libraries(cocktail-maker-ui PUBLIC cm::core)

target_sources(
    cocktail-maker-ui
    PUBLIC
        FILE_SET headers
            TYPE HEADERS
            BASE_DIRS include
            FILES
                include/cm/ui/RecipeListModel.hpp
                include/cm/ui/RecipeStoreForeign.hpp
    PRIVATE src/RecipeListModel.cpp
)
# needed for moc
target_include_directories(
    cocktail-maker-ui
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/cm/ui
)
