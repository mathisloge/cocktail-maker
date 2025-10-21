qt_add_qml_module(cocktail-maker-db
    STATIC
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CocktailMaker/Db"
    URI "CocktailMaker.Db"
    RESOURCES
        assets/mojito.png
        assets/screwdriver.png
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
    PRIVATE src/db.cpp
)
