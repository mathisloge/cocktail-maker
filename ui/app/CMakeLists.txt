qt_add_executable(CocktailMakerApp main.cpp)

target_link_libraries(
    CocktailMakerApp
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Widgets
        Qt6::QuickControls2
)

qt_add_qml_module(CocktailMakerApp
    URI "CocktailMaker.App"
    VERSION 1.0
    QML_FILES
        Main.qml
    DEPENDENCIES
        TARGET cocktail-maker-ui
)

install(
    TARGETS CocktailMakerApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
