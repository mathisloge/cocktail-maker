qt_add_qml_module(cocktail-maker-ui
    STATIC
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CocktailMaker/Ui"
    URI "CocktailMaker.Ui"
    QML_FILES
        ImageCard.qml
        Slider.qml
        Button.qml
        Section.qml
    RESOURCES
        mojito.png
)
add_library(cm::ui ALIAS cocktail-maker-ui)
